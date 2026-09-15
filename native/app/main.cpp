#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QSettings>
#include <QTimer>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QQuickStyle>
#include <QSGRendererInterface>
#include <QVariant>

#include <cstdio>
#include <memory>

#include "ui/app_controller.h"
#include "ui/mpv_quick_item.h"
#include "app/application_logger.h"

namespace {

void registerQmlTypes()
{
    static const int registered = qmlRegisterType<MpvQuickItem>(
        "DouyuNative", 1, 0, "MpvQuickItem");
    Q_UNUSED(registered);
}

int runSelfTest(QGuiApplication &application,
                QQuickWindow *window,
                const QString &mediaPath)
{
    // The root window must dispose this FBO item after its scene-graph renderer releases libmpv.
    auto *renderer = new MpvQuickItem(window->contentItem());
    renderer->setWidth(1);
    renderer->setHeight(1);
    renderer->setVisible(true);
    window->requestUpdate();

    const bool mediaRequested = !mediaPath.isEmpty();
    bool mediaStarted = !mediaRequested;
    bool mediaStopped = !mediaRequested;
    bool mediaReleased = !mediaRequested;
    QElapsedTimer rendererTimeout;
    rendererTimeout.start();
    QElapsedTimer mediaTimeout;

    QTimer probe;
    QObject::connect(&probe, &QTimer::timeout, window,
                     [&application, &probe, &rendererTimeout, &mediaTimeout, renderer, mediaPath,
                      mediaRequested,
                      &mediaStarted, &mediaStopped, &mediaReleased] {
        if (!renderer->isMpvInitialized() || !renderer->isRenderContextReady()) {
            if (rendererTimeout.elapsed() < 15000) return;
            probe.stop();
            renderer->release();
            std::fputs("native self-test could not initialize Qt Quick renderer\n", stderr);
            std::fflush(stderr);
            application.exit(1);
            return;
        }

        if (mediaRequested && !mediaStarted) {
            if (!renderer->loadLocalMedia(mediaPath)) {
                probe.stop();
                renderer->release();
                std::fputs("native self-test could not load local media\n", stderr);
                std::fflush(stderr);
                application.exit(2);
                return;
            }
            mediaStarted = true;
            mediaTimeout.start();
            return;
        }

        if (mediaRequested && !mediaStopped && !renderer->isFirstFrameRendered()) {
            if (mediaTimeout.elapsed() < 30000) return;
            probe.stop();
            renderer->release();
            std::fputs("native self-test could not render local media\n", stderr);
            std::fflush(stderr);
            application.exit(3);
            return;
        }

        if (mediaRequested && !mediaStopped) {
            if (!renderer->stop()) {
                probe.stop();
                renderer->release();
                std::fputs("native self-test could not stop local media\n", stderr);
                std::fflush(stderr);
                application.exit(4);
                return;
            }
            mediaStopped = true;
            return;
        }

        if (mediaRequested && !mediaReleased) {
            renderer->release();
            if (renderer->playbackState() != MpvQuickItem::PlaybackState::Idle
                || renderer->isMediaLoaded() || renderer->isFirstFrameRendered()) {
                probe.stop();
                std::fputs("native self-test could not release local media\n", stderr);
                std::fflush(stderr);
                application.exit(5);
                return;
            }
            mediaReleased = true;
        }

        probe.stop();
        renderer->release();
        const char *result = mediaRequested
            ? "native self-test passed: Qt Quick renderer load first-frame stop release"
            : "native self-test passed: Qt Quick renderer";
        std::fputs(result, stdout);
        std::fputc('\n', stdout);
        std::fflush(stdout);
        application.exit(0);
    });
    probe.start(20);

    return application.exec();
}

} // namespace

int main(int argc, char *argv[])
{
#ifdef Q_OS_WIN
    // The shipped Windows package contains the native Windows platform plugin.
    // Ignore test-only/headless overrides so Explorer launches remain reliable.
    qputenv("QT_QPA_PLATFORM", "windows");
#endif
    QGuiApplication application(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("DouyuMonitor"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("douyu-monitor.local"));
    QCoreApplication::setApplicationName(QStringLiteral("DouyuMonitor"));
    ApplicationLogger::install();
    qInfo() << "application started";
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Douyu Monitor native client"));
    parser.addHelpOption();
    const QCommandLineOption selfTestOption(
        QStringLiteral("self-test"),
        QStringLiteral("Load the Qt Quick renderer and verify libmpv before exiting."));
    parser.addOption(selfTestOption);
    const QCommandLineOption mediaOption(
        QStringLiteral("media"),
        QStringLiteral("Load a local media fixture during --self-test."),
        QStringLiteral("path"));
    parser.addOption(mediaOption);
    parser.process(application);

    const bool selfTest = parser.isSet(selfTestOption);
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       QStringLiteral("DouyuMonitor"), QStringLiteral("DouyuMonitor"));
    std::unique_ptr<AppController> controller;
    if (!selfTest) {
        const QDir appDir(QCoreApplication::applicationDirPath());
        const QString bundledService = appDir.filePath(QStringLiteral("streamget_service/streamget_service.exe"));
        const QString legacyService = appDir.filePath(QStringLiteral("streamget_service.exe"));
        const QString serviceProgram = QFileInfo::exists(bundledService) ? bundledService : legacyService;
        controller = std::make_unique<AppController>(serviceProgram, &settings);
    }

    registerQmlTypes();
    QQmlApplicationEngine engine;
    engine.setInitialProperties({
        {QStringLiteral("appController"),
         QVariant::fromValue(static_cast<QObject *>(controller.get()))},
    });
    engine.loadFromModule(QStringLiteral("DouyuMonitor"), QStringLiteral("Main"));
    if (engine.rootObjects().isEmpty()) return 1;

    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    if (window == nullptr) return 1;
    if (controller != nullptr) controller->setMainWindow(window);

    if (selfTest) return runSelfTest(application, window, parser.value(mediaOption));
    return application.exec();
}
