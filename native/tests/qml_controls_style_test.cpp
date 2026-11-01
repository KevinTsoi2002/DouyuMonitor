#include <QColor>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QQmlApplicationEngine>
#include <QQmlProperty>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QUrl>
#include <QtTest/QtTest>
#include <QtMath>
#include <memory>

#include "test_font_support.h"

namespace {

QQuickWindow *loadFixture(QQmlApplicationEngine &engine)
{
    engine.load(QUrl::fromLocalFile(QStringLiteral(CONTROLS_FIXTURE_PATH)));
    return engine.rootObjects().isEmpty()
        ? nullptr : qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
}

QColor backgroundColor(QObject *control)
{
    return control->property("background").value<QObject *>()->property("color").value<QColor>();
}

QPoint center(QQuickItem *item)
{
    return item->mapToScene(QPointF(item->width() / 2, item->height() / 2)).toPoint();
}

double contrast(const QColor &first, const QColor &second)
{
    const auto luminance = [](const QColor &color) {
        const auto linear = [](double channel) {
            return channel <= 0.04045 ? channel / 12.92 : qPow((channel + 0.055) / 1.055, 2.4);
        };
        return 0.2126 * linear(color.redF()) + 0.7152 * linear(color.greenF())
               + 0.0722 * linear(color.blueF());
    };
    const double a = luminance(first);
    const double b = luminance(second);
    return (qMax(a, b) + 0.05) / (qMin(a, b) + 0.05);
}

} // namespace

class QmlControlsStyleTest final : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();
    void rendersChineseTextWithoutMissingGlyphs();
    void themeHasReadableTextAndStableDimensions();
    void themeSwitchUpdatesExistingControlTextColors();
    void comboPopupSelectionRemainsReadableAcrossThemes();
    void buttonsKeepPointerKeyboardAndDisabledSemantics();
    void inputsKeepEditingAndModelRoles();
    void checkAndRadioControlsKeepNativeSelection();
    void tooltipsUseMetadataAndKeyboardFocus();
    void motionKeepsRapidInputAndReducedMode();
    void capturesSharedControls();
    void capturesIntegratedDialogs();
};

void QmlControlsStyleTest::initTestCase()
{
    QVERIFY2(DouyuTestSupport::prepareTestFonts(),
             "No Simplified Chinese font is available for QML tests (check QT_QPA_FONTDIR)");
}

void QmlControlsStyleTest::rendersChineseTextWithoutMissingGlyphs()
{
    QQmlApplicationEngine engine;
    auto *window = loadFixture(engine);
    QVERIFY(window != nullptr);
    QVERIFY(QTest::qWaitForWindowExposed(window));
    QVERIFY(window->setProperty("reducedMotion", true));
    auto *button = window->findChild<QQuickItem *>("button");
    QVERIFY(button != nullptr);
    button->setWidth(120);
    const QFont controlFont = button->property("font").value<QFont>();
    QList<QFont> fonts{QGuiApplication::font(), controlFont};
    for (const char *family : {"Microsoft YaHei", "SimHei"}) {
        QFont font = controlFont;
        font.setFamily(QString::fromLatin1(family));
        fonts.append(font);
    }
    for (const QFont &font : fonts) {
        QVERIFY(button->setProperty("font", font));
        QVERIFY(button->setProperty("text", QStringLiteral("设置")));
        QTest::qWait(50);
        const QRect rect = button->mapRectToScene(button->boundingRect()).toAlignedRect();
        const QImage settings = window->grabWindow().copy(rect);
        QVERIFY(!settings.isNull());
        QVERIFY(button->setProperty("text", QStringLiteral("直播")));
        QTest::qWait(50);
        const QImage live = window->grabWindow().copy(rect);
        QVERIFY(!live.isNull());
        QVERIFY2(settings != live,
                 qPrintable("Distinct Chinese labels render as identical boxes in " + font.family()));
    }
}

void QmlControlsStyleTest::themeHasReadableTextAndStableDimensions()
{
    QQmlApplicationEngine engine;
    QSignalSpy warnings(&engine, &QQmlEngine::warnings);
    auto *window = loadFixture(engine);
    QVERIFY(window != nullptr);
    QCOMPARE(warnings.count(), 0);
    const QDir fixtureDirectory(QFileInfo(QStringLiteral(CONTROLS_FIXTURE_PATH)).absolutePath());
    const QString themeDirectory = QUrl::fromLocalFile(
        fixtureDirectory.absoluteFilePath("../../app/qml")).toString();
    QQmlComponent themeProbe(&engine);
    themeProbe.setData(QStringLiteral(
        "import QtQuick\n"
        "import \"%1\"\n"
        "QtObject { property int pageTitleSize: Theme.fontPageTitle; property string mode: 'night'; onModeChanged: Theme.mode = mode }\n")
                           .arg(themeDirectory)
                           .toUtf8(),
                       QUrl(QStringLiteral("file:///ThemeProbe.qml")));
    QVERIFY2(themeProbe.isReady(), qPrintable(themeProbe.errorString()));
    std::unique_ptr<QObject> theme(themeProbe.create());
    QVERIFY2(theme != nullptr, qPrintable(themeProbe.errorString()));
    QCOMPARE(theme->property("pageTitleSize").toInt(), 24);
    QCOMPARE(window->color(), QColor("#0e1312"));
    QCOMPARE(window->property("onlineColor").value<QColor>(), QColor("#76d9a0"));
    for (const char *name : {"button", "primary", "selected", "tool", "field", "combo", "check", "radioA"}) {
        auto *item = window->findChild<QQuickItem *>(QString::fromLatin1(name));
        QVERIFY(item != nullptr);
        QCOMPARE(item->height(), 32.0);
    }
    QVERIFY(contrast(window->property("primaryText").value<QColor>(), QColor("#1e2624")) >= 4.5);
    QVERIFY(contrast(window->property("secondaryText").value<QColor>(), QColor("#1e2624")) >= 4.5);
    auto *primary = window->findChild<QObject *>("primary");
    auto *label = primary->property("contentItem").value<QObject *>();
    QVERIFY(label != nullptr);
    QVERIFY(contrast(label->property("color").value<QColor>(), backgroundColor(primary)) >= 4.5);
    auto *disabledPrimary = window->findChild<QObject *>("disabledPrimary");
    QVERIFY(disabledPrimary != nullptr);
    auto *disabledLabel = disabledPrimary->property("contentItem").value<QObject *>();
    QVERIFY(disabledLabel != nullptr);
    QTRY_VERIFY2(contrast(disabledLabel->property("color").value<QColor>(), backgroundColor(disabledPrimary)) >= 4.5,
                 qPrintable(disabledLabel->property("color").value<QColor>().name()));
    QVERIFY(theme->setProperty("mode", QStringLiteral("day")));
    QTRY_COMPARE(window->color(), QColor("#f0f3ed"));
    QCOMPARE(window->property("primaryText").value<QColor>(), QColor("#1b281b"));
    QCOMPARE(window->property("secondaryText").value<QColor>(), QColor("#465446"));
    QVERIFY(contrast(window->property("primaryText").value<QColor>(), QColor("#e8ede4")) >= 4.5);
    QVERIFY(contrast(window->property("secondaryText").value<QColor>(), QColor("#e8ede4")) >= 4.5);
    QTRY_VERIFY(contrast(label->property("color").value<QColor>(), backgroundColor(primary)) >= 4.5);
    QVERIFY(primary->setProperty("enabled", false));
    QTRY_VERIFY(contrast(label->property("color").value<QColor>(), backgroundColor(primary)) >= 4.5);
    QVERIFY(theme->setProperty("mode", QStringLiteral("night")));
    QTRY_VERIFY(contrast(label->property("color").value<QColor>(), backgroundColor(primary)) >= 4.5);
}

void QmlControlsStyleTest::themeSwitchUpdatesExistingControlTextColors()
{
    QQmlApplicationEngine engine;
    QSignalSpy warnings(&engine, &QQmlEngine::warnings);
    auto *window = loadFixture(engine);
    QVERIFY(window != nullptr);
    QVERIFY(QTest::qWaitForWindowExposed(window));
    QVERIFY(window->setProperty("reducedMotion", true));
    window->contentItem()->forceActiveFocus(Qt::OtherFocusReason);

    const QDir fixtureDirectory(QFileInfo(QStringLiteral(CONTROLS_FIXTURE_PATH)).absolutePath());
    const QString themeDirectory = QUrl::fromLocalFile(
        fixtureDirectory.absoluteFilePath("../../app/qml")).toString();
    QQmlComponent themeProbe(&engine);
    themeProbe.setData(QStringLiteral(
        "import QtQuick\n"
        "import \"%1\"\n"
        "QtObject { property string mode: 'night'; onModeChanged: Theme.mode = mode }\n")
                          .arg(themeDirectory).toUtf8(),
                      QUrl(QStringLiteral("file:///ThemeTransitionProbe.qml")));
    QVERIFY2(themeProbe.isReady(), qPrintable(themeProbe.errorString()));
    std::unique_ptr<QObject> theme(themeProbe.create());
    QVERIFY2(theme != nullptr, qPrintable(themeProbe.errorString()));

    QList<QObject *> controls;
    for (const char *name : {"button", "primary", "selected", "tool", "selectedTool", "combo", "check", "radioA"}) {
        auto *control = window->findChild<QObject *>(QString::fromLatin1(name));
        QVERIFY(control != nullptr);
        QVERIFY(control->property("contentItem").value<QObject *>() != nullptr);
        controls.append(control);
    }
    auto *field = window->findChild<QObject *>("field");
    auto *disabledPrimary = window->findChild<QObject *>("disabledPrimary");
    auto *disabledTool = window->findChild<QObject *>("disabledTool");
    auto *tooltip = window->findChild<QObject *>("tooltip");
    QVERIFY(field && disabledPrimary && disabledTool && tooltip);

    for (const bool enabled : {true, false, true}) {
        for (auto *control : controls)
            QVERIFY(control->setProperty("enabled", enabled));
        QVERIFY(field->setProperty("enabled", enabled));

        for (const char *mode : {"night", "day", "night"}) {
            const bool day = qstrcmp(mode, "day") == 0;
            const QColor text(day ? "#1b281b" : "#e9f0ee");
            const QColor disabledText(day ? "#525f52" : "#9aa8a3");
            const QColor accentText(day ? "#fbfcfa" : "#071713");
            const QColor accent(day ? "#295a2c" : "#59dbb9");
            QVERIFY(theme->setProperty("mode", QString::fromLatin1(mode)));

            for (auto *control : controls) {
                const QColor expected = !enabled ? disabledText
                    : control->objectName() == QStringLiteral("primary") ? accentText : text;
                auto *content = control->property("contentItem").value<QObject *>();
                QTRY_COMPARE(content->property("color").value<QColor>(), expected);
                QTRY_COMPARE(QQmlProperty::read(control, "palette.buttonText").value<QColor>(), expected);
                if (control->objectName() == QStringLiteral("button")
                    || control->objectName() == QStringLiteral("primary")
                    || control->objectName() == QStringLiteral("tool")) {
                    QTRY_COMPARE(QQmlProperty::read(control, "icon.color").value<QColor>(), expected);
                }
            }

            auto *disabledContent = disabledPrimary->property("contentItem").value<QObject *>();
            QVERIFY(disabledContent != nullptr);
            QTRY_COMPARE(disabledContent->property("color").value<QColor>(), disabledText);
            auto *disabledToolContent = disabledTool->property("contentItem").value<QObject *>();
            QVERIFY(disabledToolContent != nullptr);
            QTRY_COMPARE(disabledToolContent->property("color").value<QColor>(), disabledText);
            QTRY_COMPARE(QQmlProperty::read(disabledTool, "icon.color").value<QColor>(), disabledText);
            QTRY_COMPARE(field->property("color").value<QColor>(), enabled ? text : disabledText);
            QTRY_COMPARE(QQmlProperty::read(field, "palette.highlight").value<QColor>(), accent);
            auto *tooltipContent = tooltip->property("contentItem").value<QObject *>();
            QVERIFY(tooltipContent != nullptr);
            QTRY_COMPARE(tooltipContent->property("color").value<QColor>(), text);
            QTRY_COMPARE(QQmlProperty::read(tooltip, "palette.toolTipText").value<QColor>(), text);
        }
    }
    QCOMPARE(warnings.count(), 0);
}

void QmlControlsStyleTest::comboPopupSelectionRemainsReadableAcrossThemes()
{
    QQmlApplicationEngine engine;
    QSignalSpy warnings(&engine, &QQmlEngine::warnings);
    auto *window = loadFixture(engine);
    QVERIFY(window != nullptr);
    QVERIFY(QTest::qWaitForWindowExposed(window));
    window->requestActivate();
    QVERIFY(window->setProperty("reducedMotion", true));
    const QDir fixtureDirectory(QFileInfo(QStringLiteral(CONTROLS_FIXTURE_PATH)).absolutePath());
    const QString themeDirectory = QUrl::fromLocalFile(
        fixtureDirectory.absoluteFilePath("../../app/qml")).toString();
    QQmlComponent themeProbe(&engine);
    themeProbe.setData(QStringLiteral(
        "import QtQuick\n"
        "import \"%1\"\n"
        "QtObject { property string mode: 'night'; onModeChanged: Theme.mode = mode }\n")
                          .arg(themeDirectory).toUtf8(),
                      QUrl(QStringLiteral("file:///PopupContrastProbe.qml")));
    QVERIFY2(themeProbe.isReady(), qPrintable(themeProbe.errorString()));
    std::unique_ptr<QObject> theme(themeProbe.create());
    QVERIFY2(theme != nullptr, qPrintable(themeProbe.errorString()));
    auto *combo = window->findChild<QQuickItem *>("combo");
    QVERIFY(combo != nullptr);
    auto *popup = combo->property("popup").value<QObject *>();
    QVERIFY(popup != nullptr);
    auto *list = popup->property("contentItem").value<QObject *>();
    QVERIFY(list != nullptr);
    const QString output = QCoreApplication::applicationDirPath()
                           + "/verification/theme-controls/" + QGuiApplication::platformName();
    QVERIFY(QDir().mkpath(output));

    for (const char *mode : {"night", "day", "night"}) {
        const bool day = qstrcmp(mode, "day") == 0;
        QVERIFY(theme->setProperty("mode", QString::fromLatin1(mode)));
        QVERIFY(combo->setProperty("currentIndex", 0));
        combo->forceActiveFocus(Qt::TabFocusReason);
        QVERIFY(QMetaObject::invokeMethod(popup, "open"));
        QTRY_VERIFY(popup->property("opened").toBool());
        QTRY_VERIFY(list->property("currentItem").value<QObject *>() != nullptr);
        auto *row = list->property("currentItem").value<QObject *>();
        auto *label = row->property("contentItem").value<QObject *>();
        QVERIFY(label != nullptr);
        QCOMPARE(row->property("text").toString(), QString("First"));
        QTRY_COMPARE(label->property("color").value<QColor>(), QColor(day ? "#1b281b" : "#e9f0ee"));
        QTRY_COMPARE(backgroundColor(row), QColor(day ? "#d4e4cd" : "#1a342c"));
        QVERIFY(contrast(label->property("color").value<QColor>(), backgroundColor(row)) >= 4.5);

        // Moving the popup highlight is provisional until Return commits the model value.
        QTest::keyClick(window, Qt::Key_Down);
        QTRY_COMPARE(combo->property("highlightedIndex").toInt(), 1);
        QCOMPARE(combo->property("currentValue").toString(), QString("first"));
        row = list->property("currentItem").value<QObject *>();
        QVERIFY(row != nullptr);
        label = row->property("contentItem").value<QObject *>();
        QVERIFY(label != nullptr);
        QCOMPARE(row->property("text").toString(), QString("Second"));
        QVERIFY(contrast(label->property("color").value<QColor>(), backgroundColor(row)) >= 4.5);
        const QImage capture = window->grabWindow();
        QVERIFY(!capture.isNull());
        QVERIFY(capture.save(output + "/combo-" + QString::fromLatin1(mode) + ".png"));
        QTest::keyClick(window, Qt::Key_Return);
        QTRY_VERIFY(!popup->property("visible").toBool());
        QCOMPARE(combo->property("currentValue").toString(), QString("second"));
    }
    QCOMPARE(warnings.count(), 0);
}

void QmlControlsStyleTest::buttonsKeepPointerKeyboardAndDisabledSemantics()
{
    QQmlApplicationEngine engine;
    QSignalSpy warnings(&engine, &QQmlEngine::warnings);
    auto *window = loadFixture(engine);
    QVERIFY(window != nullptr);
    QVERIFY(QTest::qWaitForWindowExposed(window));
    auto *button = window->findChild<QQuickItem *>("button");
    QVERIFY(button != nullptr);
    QSignalSpy clicked(button, SIGNAL(clicked()));
    QCOMPARE(backgroundColor(button), QColor("#1e2624"));
    const QSizeF initialSize(button->width(), button->height());
    QTest::mouseMove(window, center(button));
    QTRY_VERIFY(button->property("hovered").toBool());
    QTRY_COMPARE(backgroundColor(button), QColor("#25312e"));
    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, center(button));
    QVERIFY(button->property("down").toBool());
    QTRY_COMPARE(backgroundColor(button), QColor("#161c1b"));
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, center(button));
    QCOMPARE(clicked.count(), 1);
    window->requestActivate();
    QTRY_VERIFY(window->isActive());
    window->findChild<QQuickItem *>("field")->forceActiveFocus();
    button->forceActiveFocus(Qt::TabFocusReason);
    QTRY_VERIFY(button->property("visualFocus").toBool());
    auto *background = button->property("background").value<QObject *>();
    QTRY_COMPARE(QQmlProperty::read(background, "border.color").value<QColor>(),
                 QColor("#59dbb9"));
    QTest::keyClick(window, Qt::Key_Space);
    QCOMPARE(clicked.count(), 2);
    button->setEnabled(false);
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, center(button));
    QCOMPARE(clicked.count(), 2);
    QTRY_COMPARE(backgroundColor(button), QColor("#1e2624"));
    auto *label = button->property("contentItem").value<QObject *>();
    QCOMPARE(label->property("color").value<QColor>(), QColor("#9aa8a3"));
    auto *tool = window->findChild<QObject *>("tool");
    QVERIFY(tool != nullptr);
    tool->setProperty("enabled", false);
    auto *toolLabel = tool->property("contentItem").value<QObject *>();
    QCOMPARE(toolLabel->property("color").value<QColor>(), QColor("#9aa8a3"));
    QCOMPARE(QQmlProperty::read(tool, "icon.color").value<QColor>(), QColor("#9aa8a3"));
    auto *selected = window->findChild<QQuickItem *>("selected");
    QVERIFY(selected != nullptr);
    QTest::mouseMove(window, center(selected));
    QTRY_VERIFY(selected->property("hovered").toBool());
    QCOMPARE(backgroundColor(selected), QColor("#1a342c"));
    QCOMPARE(QSizeF(button->width(), button->height()), initialSize);
    QCOMPARE(warnings.count(), 0);
}

void QmlControlsStyleTest::inputsKeepEditingAndModelRoles()
{
    QQmlApplicationEngine engine;
    QSignalSpy warnings(&engine, &QQmlEngine::warnings);
    auto *window = loadFixture(engine);
    QVERIFY(window != nullptr);
    QVERIFY(QTest::qWaitForWindowExposed(window));
    auto *field = window->findChild<QQuickItem *>("field");
    QVERIFY(field != nullptr);
    QSignalSpy accepted(field, SIGNAL(accepted()));
    field->forceActiveFocus();
    QTRY_VERIFY(field->property("activeFocus").toBool());
    auto *fieldBackground = field->property("background").value<QObject *>();
    QVERIFY(fieldBackground != nullptr);
    QCOMPARE(QQmlProperty::read(fieldBackground, "border.width").toInt(), 2);
    for (char key : QByteArray("room123")) QTest::keyClick(window, key);
    QTest::keyClick(window, Qt::Key_Return);
    QCOMPARE(field->property("text").toString(), QString("room123"));
    QCOMPARE(accepted.count(), 1);

    auto *combo = window->findChild<QQuickItem *>("combo");
    QVERIFY(combo != nullptr);
    QCOMPARE(combo->property("currentValue").toString(), QString("second"));
    QCOMPARE(combo->property("displayText").toString(), QString("Second"));
    combo->forceActiveFocus(Qt::TabFocusReason);
    QTRY_VERIFY(combo->property("activeFocus").toBool());
    auto *comboBackground = combo->property("background").value<QObject *>();
    QVERIFY(comboBackground != nullptr);
    QCOMPARE(QQmlProperty::read(comboBackground, "border.width").toInt(), 2);
    QSignalSpy activated(combo, SIGNAL(activated(int)));
    QTest::keyClick(window, Qt::Key_Up);
    QCOMPARE(combo->property("currentValue").toString(), QString("first"));
    QCOMPARE(activated.count(), 1);
    auto *popup = combo->property("popup").value<QObject *>();
    QVERIFY(popup != nullptr);
    const QString valueBeforeEscape = combo->property("currentValue").toString();
    QVERIFY(QMetaObject::invokeMethod(popup, "open"));
    QTRY_VERIFY(popup->property("visible").toBool());
    QCOMPARE(backgroundColor(popup), QColor("#0e1312"));
    QTest::keyClick(window, Qt::Key_Escape);
    QTRY_VERIFY(!popup->property("visible").toBool());
    QCOMPARE(combo->property("currentValue").toString(), valueBeforeEscape);
    combo->setProperty("editable", true);
    auto *input = combo->property("contentItem").value<QQuickItem *>();
    QVERIFY(input != nullptr);
    input->forceActiveFocus();
    QTest::keyClick(window, Qt::Key_A, Qt::ControlModifier);
    for (char key : QByteArray("Custom")) QTest::keyClick(window, key);
    QCOMPARE(combo->property("editText").toString(), QString("Custom"));
    QCOMPARE(warnings.count(), 0);
}

void QmlControlsStyleTest::checkAndRadioControlsKeepNativeSelection()
{
    QQmlApplicationEngine engine;
    auto *window = loadFixture(engine);
    QVERIFY(window != nullptr);
    QVERIFY(QTest::qWaitForWindowExposed(window));
    window->requestActivate();
    QTRY_VERIFY(window->isActive());
    auto *check = window->findChild<QQuickItem *>("check");
    auto *partial = window->findChild<QQuickItem *>("partial");
    auto *radioA = window->findChild<QQuickItem *>("radioA");
    auto *radioB = window->findChild<QQuickItem *>("radioB");
    QVERIFY(check && partial && radioA && radioB);
    auto *checkIndicator = check->property("indicator").value<QObject *>();
    auto *radioAIndicator = radioA->property("indicator").value<QObject *>();
    auto *radioBIndicator = radioB->property("indicator").value<QObject *>();
    QVERIFY(checkIndicator && radioAIndicator && radioBIndicator);

    QSignalSpy toggled(check, SIGNAL(toggled()));
    QTest::mouseMove(window, QPoint(620, 460));
    QTRY_VERIFY(!check->property("hovered").toBool());
    const QColor checkNormalBorder = QQmlProperty::read(checkIndicator, "border.color").value<QColor>();
    QTest::mouseMove(window, center(check));
    QTRY_VERIFY(check->property("hovered").toBool());
    QVERIFY(QQmlProperty::read(checkIndicator, "border.color").value<QColor>() != checkNormalBorder);
    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, center(check));
    QVERIFY(check->property("down").toBool());
    QCOMPARE(QQmlProperty::read(checkIndicator, "color").value<QColor>(), QColor("#161c1b"));
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, center(check));
    QVERIFY(check->property("checked").toBool());
    QCOMPARE(toggled.count(), 1);
    const QColor checkCheckedColor = QQmlProperty::read(checkIndicator, "color").value<QColor>();
    QTest::mouseMove(window, QPoint(620, 460));
    QTRY_VERIFY(!check->property("hovered").toBool());
    QTest::mouseMove(window, center(check));
    QTRY_VERIFY(check->property("hovered").toBool());
    QCOMPARE(QQmlProperty::read(checkIndicator, "color").value<QColor>(), checkCheckedColor);
    QTest::mouseMove(window, QPoint(620, 460));
    check->forceActiveFocus(Qt::TabFocusReason);
    QTRY_VERIFY(check->property("activeFocus").toBool());
    QCOMPARE(QQmlProperty::read(checkIndicator, "border.width").toInt(), 2);
    check->setEnabled(false);
    QCOMPARE(QQmlProperty::read(checkIndicator, "color").value<QColor>(), QColor("#1e2624"));

    QCOMPARE(partial->property("checkState").toInt(), int(Qt::PartiallyChecked));
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, center(partial));
    QCOMPARE(partial->property("checkState").toInt(), int(Qt::Checked));

    QTest::mouseMove(window, QPoint(620, 460));
    QTRY_VERIFY(!radioB->property("hovered").toBool());
    const QColor radioBNormalBorder = QQmlProperty::read(radioBIndicator, "border.color").value<QColor>();
    QTest::mouseMove(window, center(radioB));
    QTRY_VERIFY(radioB->property("hovered").toBool());
    QVERIFY(QQmlProperty::read(radioBIndicator, "border.color").value<QColor>() != radioBNormalBorder);
    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, center(radioB));
    QVERIFY(radioB->property("down").toBool());
    QTRY_COMPARE(backgroundColor(radioB), QColor("#161c1b"));
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, center(radioB));
    QVERIFY(radioB->property("checked").toBool());
    const QColor radioBCheckedBorder =
        QQmlProperty::read(radioBIndicator, "border.color").value<QColor>();
    QTest::mouseMove(window, center(radioB));
    QTRY_VERIFY(radioB->property("hovered").toBool());
    QCOMPARE(QQmlProperty::read(radioBIndicator, "border.color").value<QColor>(),
             radioBCheckedBorder);
    radioB->forceActiveFocus(Qt::TabFocusReason);
    QTRY_VERIFY(radioB->property("activeFocus").toBool());
    QCOMPARE(QQmlProperty::read(radioBIndicator, "border.width").toInt(), 2);
    radioA->setEnabled(false);
    QCOMPARE(QQmlProperty::read(radioAIndicator, "color").value<QColor>(), QColor("#1e2624"));
    QVERIFY(!radioA->property("checked").toBool());
}

void QmlControlsStyleTest::tooltipsUseMetadataAndKeyboardFocus()
{
    QQmlApplicationEngine engine;
    QSignalSpy warnings(&engine, &QQmlEngine::warnings);
    auto *window = loadFixture(engine);
    QVERIFY(window != nullptr);
    QVERIFY(QTest::qWaitForWindowExposed(window));
    window->requestActivate();
    QTRY_VERIFY(window->isActive());
    auto *tool = window->findChild<QQuickItem *>("tool");
    QVERIFY(tool != nullptr);
    auto *tooltip = tool->findChild<QObject *>("consoleToolTip");
    QVERIFY(tooltip != nullptr);
    QCOMPARE(tooltip->property("text").toString(), QString("Close"));
    auto *field = window->findChild<QQuickItem *>("field");
    QVERIFY(field != nullptr);
    field->forceActiveFocus(Qt::TabFocusReason);
    QTest::mouseMove(window, QPoint(620, 460));
    QTRY_VERIFY(!tooltip->property("visible").toBool());
    QTest::mouseMove(window, center(tool));
    QTRY_VERIFY(tool->property("hovered").toBool());
    QTest::qWait(650);
    QVERIFY(!tooltip->property("visible").toBool());
    QTest::mouseMove(window, QPoint(620, 460));
    QTRY_VERIFY(!tooltip->property("visible").toBool());
    tool->forceActiveFocus(Qt::TabFocusReason);
    QCOMPARE(tooltip->property("delay").toInt(), 0);
    QTRY_VERIFY(tooltip->property("visible").toBool());
    QCOMPARE(backgroundColor(tooltip), QColor("#1e2624"));
    tool->setEnabled(false);
    QTRY_VERIFY(!tooltip->property("visible").toBool());
    QCOMPARE(warnings.count(), 0);
}

void QmlControlsStyleTest::capturesSharedControls()
{
    QQmlApplicationEngine engine;
    auto *window = loadFixture(engine);
    QVERIFY(window != nullptr);
    QVERIFY(QTest::qWaitForWindowExposed(window));
    QTest::qWait(100);
    const QImage image = window->grabWindow();
    QVERIFY(!image.isNull());
    const auto directory = QCoreApplication::applicationDirPath() + "/verification/theme-controls/"
                           + QGuiApplication::platformName();
    QVERIFY(QDir().mkpath(directory));
    QVERIFY(image.save(directory + "/controls.png"));
    auto *primary = window->findChild<QQuickItem *>("primary");
    QVERIFY(primary != nullptr);
    const QPoint pixel = (primary->mapToScene(QPointF(5, 5)) * window->devicePixelRatio()).toPoint();
    QCOMPARE(image.pixelColor(pixel), QColor("#59dbb9"));
}

void QmlControlsStyleTest::motionKeepsRapidInputAndReducedMode()
{
    QQmlApplicationEngine engine;
    QSignalSpy warnings(&engine, &QQmlEngine::warnings);
    auto *window = loadFixture(engine);
    QVERIFY(window != nullptr);
    QVERIFY(QTest::qWaitForWindowExposed(window));
    QCOMPARE(window->property("motionDuration").toInt(), 150);
    auto *button = window->findChild<QQuickItem *>("button");
    auto *combo = window->findChild<QQuickItem *>("combo");
    QVERIFY(button && combo);
    const QSizeF buttonSize(button->width(), button->height());
    QSignalSpy clicked(button, SIGNAL(clicked()));
    for (int index = 0; index < 4; ++index)
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, center(button));
    QCOMPARE(clicked.count(), 4);
    QCOMPARE(QSizeF(button->width(), button->height()), buttonSize);
    QTRY_COMPARE(backgroundColor(button), QColor("#25312e"));

    auto *popup = combo->property("popup").value<QObject *>();
    QVERIFY(popup != nullptr);
    for (int index = 0; index < 3; ++index) {
        QVERIFY(QMetaObject::invokeMethod(popup, "open"));
        QTRY_VERIFY(popup->property("visible").toBool());
        QVERIFY(QMetaObject::invokeMethod(popup, "close"));
        QTRY_VERIFY(!popup->property("visible").toBool());
    }
    QVERIFY(QMetaObject::invokeMethod(popup, "open"));
    QTRY_VERIFY(popup->property("opened").toBool());
    combo->forceActiveFocus();
    QTest::keyClick(window, Qt::Key_Escape);
    QTRY_VERIFY(!popup->property("visible").toBool());
    QCOMPARE(combo->property("currentValue").toString(), QString("second"));

    QVERIFY(window->setProperty("reducedMotion", true));
    QCOMPARE(window->property("motionDuration").toInt(), 0);
    QTest::mouseMove(window, QPoint(620, 460));
    QTRY_VERIFY(!button->property("hovered").toBool());
    QTRY_COMPARE(backgroundColor(button), QColor("#1e2624"));
    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, center(button));
    QCOMPARE(backgroundColor(button), QColor("#161c1b"));
    QCOMPARE(clicked.count(), 4);
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, center(button));
    QCOMPARE(clicked.count(), 5);
    QCOMPARE(warnings.count(), 0);
}

void QmlControlsStyleTest::capturesIntegratedDialogs()
{
    const QDir fixtureDirectory(QFileInfo(QStringLiteral(CONTROLS_FIXTURE_PATH)).absolutePath());
    const auto output = QCoreApplication::applicationDirPath() + "/verification/theme-controls/"
                        + QGuiApplication::platformName();
    QVERIFY(QDir().mkpath(output));
    for (const char *name : {"AddRoomDialog", "CloseBehaviorDialog", "NotificationSettingsDialog",
                             "TeamManagerDialog"}) {
        QQmlApplicationEngine engine;
        QSignalSpy warnings(&engine, &QQmlEngine::warnings);
        QQuickWindow window;
        window.resize(960, 720);
        window.setColor(QColor("#0e1312"));
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QQmlComponent component(&engine, QUrl::fromLocalFile(fixtureDirectory.absoluteFilePath(
            "../../app/qml/dialogs/" + QString::fromLatin1(name) + ".qml")));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        std::unique_ptr<QObject> dialog(component.createWithInitialProperties({
            {"parent", QVariant::fromValue(window.contentItem())}}));
        QVERIFY2(dialog != nullptr, qPrintable(component.errorString()));
        QVERIFY(QMetaObject::invokeMethod(dialog.get(), "open"));
        QTRY_VERIFY(dialog->property("visible").toBool());
        QTest::qWait(100);
        const QImage image = window.grabWindow();
        QVERIFY(!image.isNull());
        QVERIFY(image.save(output + "/" + QString::fromLatin1(name) + ".png"));
        QCOMPARE(warnings.count(), 0);
        QVERIFY(QMetaObject::invokeMethod(dialog.get(), "close"));
    }
}

QTEST_MAIN(QmlControlsStyleTest)
#include "qml_controls_style_test.moc"
