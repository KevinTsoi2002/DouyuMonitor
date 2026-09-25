#include "visual_test_support.h"

#include <QColor>
#include <QDir>
#include <QQuickItem>
#include <QQuickWindow>
#include <QtGlobal>


namespace {

QString outputPath(const QString &directory, const QString &name)
{
    return directory + QLatin1Char('/') + name + QStringLiteral(".png");
}

int channelDelta(int first, int second)
{
    return qAbs(first - second);
}

bool visuallyDifferent(const QColor &first, const QColor &second, int threshold)
{
    return channelDelta(first.red(), second.red()) > threshold
           || channelDelta(first.green(), second.green()) > threshold
           || channelDelta(first.blue(), second.blue()) > threshold
           || channelDelta(first.alpha(), second.alpha()) > threshold;
}

QString comparisonMessage(const QString &name,
                          int changedPixels,
                          double changedRatio,
                          const QString &actualPath,
                          const QString &diffPath)
{
    return QStringLiteral("Visual baseline mismatch for %1: %2 pixels (%3%) differ. "
                          "Current: %4. Diff: %5. Review the change, then run "
                          "scripts/update-visual-baselines.ps1 -Approve if intentional.")
        .arg(name)
        .arg(changedPixels)
        .arg(changedRatio * 100.0, 0, 'f', 3)
        .arg(actualPath)
        .arg(diffPath);
}

} // namespace

QQuickItem *visualItemByObjectName(QQuickItem *root, const QString &objectName)
{
    if (root == nullptr) return nullptr;
    if (root->objectName() == objectName) return root;
    for (QQuickItem *child : root->childItems()) {
        if (QQuickItem *match = visualItemByObjectName(child, objectName)) return match;
    }
    return nullptr;
}

QRectF visualSceneRect(QQuickItem *item)
{
    if (item == nullptr) return {};
    return {item->mapToScene(QPointF(0, 0)), QSizeF(item->width(), item->height())};
}

bool visualRectsOverlap(const QRectF &first, const QRectF &second, qreal tolerance)
{
    const QRectF intersection = first.intersected(second);
    return intersection.width() > tolerance && intersection.height() > tolerance;
}

QVariantList visualRoomFixtures(int count, bool multiAudio, bool includeQualities)
{
    QVariantList rooms;
    for (int index = 0; index < count; ++index) {
        QVariantList qualities;
        if (includeQualities) {
            qualities = {
                QVariantMap{{QStringLiteral("id"), QStringLiteral("auto")},
                            {QStringLiteral("label"), QStringLiteral("自动")},
                            {QStringLiteral("rate"), -1}},
                QVariantMap{{QStringLiteral("id"), QStringLiteral("rate-4")},
                            {QStringLiteral("label"), QStringLiteral("蓝光")},
                            {QStringLiteral("rate"), 4}},
                QVariantMap{{QStringLiteral("id"), QStringLiteral("rate-3")},
                            {QStringLiteral("label"), QStringLiteral("高清")},
                            {QStringLiteral("rate"), 3}},
                QVariantMap{{QStringLiteral("id"), QStringLiteral("rate-2")},
                            {QStringLiteral("label"), QStringLiteral("标清")},
                            {QStringLiteral("rate"), 2}},
            };
        }

        rooms.push_back(QVariantMap{
            {QStringLiteral("roomId"), QStringLiteral("fixture-%1").arg(index + 1)},
            {QStringLiteral("anchorName"), QStringLiteral("验收主播 %1").arg(index + 1)},
            {QStringLiteral("title"), QStringLiteral("多路画布视觉回归直播间 %1").arg(index + 1)},
            {QStringLiteral("category"), QStringLiteral("视觉回归")},
            {QStringLiteral("viewerLabel"), QStringLiteral("1.2万")},
            {QStringLiteral("avatarUrl"), QUrl()},
            {QStringLiteral("liveState"), index % 2 == 0
                                              ? QStringLiteral("online")
                                              : QStringLiteral("offline")},
            {QStringLiteral("playbackState"), QStringLiteral("idle")},
            {QStringLiteral("primary"), index == 0},
            {QStringLiteral("favorite"), false},
            {QStringLiteral("audioFocused"), multiAudio && index == 0},
            {QStringLiteral("requestedQuality"), QStringLiteral("auto")},
            {QStringLiteral("requestedQualityRate"), -1},
            {QStringLiteral("effectiveQuality"), QStringLiteral("auto")},
            {QStringLiteral("availableQualities"), qualities},
            {QStringLiteral("muted"), false},
            {QStringLiteral("volume"), 100},
            {QStringLiteral("danmakuEnabled"), false},
            {QStringLiteral("danmakuState"), QStringLiteral("idle")},
            {QStringLiteral("danmakuErrorCode"), QStringLiteral("NONE")},
            {QStringLiteral("index"), index},
        });
    }
    return rooms;
}

QImage visualCapture(QQuickWindow *window, const QString &name)
{
    if (window == nullptr) return {};
    const QImage image = window->grabWindow();
    const QString directory = QStringLiteral(QML_VISUAL_OUTPUT_DIR);
    if (!QDir().mkpath(directory)) return {};
    if (!image.save(outputPath(directory, name))) return {};
    return image;
}

VisualComparisonResult visualCompareWithBaseline(const QImage &image,
                                                 const QString &name,
                                                 const QVector<QRect> &ignoredRegions)
{
    VisualComparisonResult result;
    if (image.isNull()) {
        result.message = QStringLiteral("Visual capture is null for %1").arg(name);
        return result;
    }

    const QString baselineDirectory = QStringLiteral(QML_VISUAL_BASELINE_DIR);
    const QString outputDirectory = QStringLiteral(QML_VISUAL_OUTPUT_DIR);
    const QString baselinePath = outputPath(baselineDirectory, name);
    const QString actualPath = outputPath(outputDirectory, name);
    const QString diffPath = outputPath(outputDirectory, name + QStringLiteral("-diff"));

    if (qEnvironmentVariableIsSet("DOUYU_UPDATE_VISUAL_BASELINES")) {
        if (!QDir().mkpath(baselineDirectory)) {
            result.message = QStringLiteral("Could not create visual baseline directory: %1")
                                 .arg(baselineDirectory);
            return result;
        }
        result.ok = image.save(baselinePath);
        result.message = result.ok
            ? QStringLiteral("Updated visual baseline: %1").arg(baselinePath)
            : QStringLiteral("Could not save visual baseline: %1").arg(baselinePath);
        return result;
    }

    QImage baseline(baselinePath);
    if (baseline.isNull()) {
        result.message = QStringLiteral("Visual baseline is missing: %1. "
                                        "Capture review is required before updating it.")
                             .arg(baselinePath);
        return result;
    }
    if (baseline.size() != image.size()) {
        result.message = QStringLiteral("Visual baseline size mismatch for %1: baseline %2x%3, "
                                        "current %4x%5.")
                             .arg(name)
                             .arg(baseline.width())
                             .arg(baseline.height())
                             .arg(image.width())
                             .arg(image.height());
        return result;
    }

    QImage current = image.convertToFormat(QImage::Format_ARGB32);
    baseline = baseline.convertToFormat(QImage::Format_ARGB32);
    for (const QRect &region : ignoredRegions) {
        const QRect bounded = region.intersected(current.rect());
        if (bounded.isEmpty()) continue;
        for (int y = bounded.top(); y <= bounded.bottom(); ++y) {
            auto *currentLine = reinterpret_cast<QRgb *>(current.scanLine(y));
            auto *baselineLine = reinterpret_cast<QRgb *>(baseline.scanLine(y));
            for (int x = bounded.left(); x <= bounded.right(); ++x) {
                currentLine[x] = qRgba(0, 0, 0, 0);
                baselineLine[x] = qRgba(0, 0, 0, 0);
            }
        }
    }

    QImage diff(current.size(), QImage::Format_ARGB32);
    diff.fill(Qt::transparent);
    constexpr int channelThreshold = 16;
    for (int y = 0; y < current.height(); ++y) {
        const auto *currentLine = reinterpret_cast<const QRgb *>(current.constScanLine(y));
        const auto *baselineLine = reinterpret_cast<const QRgb *>(baseline.constScanLine(y));
        auto *diffLine = reinterpret_cast<QRgb *>(diff.scanLine(y));
        for (int x = 0; x < current.width(); ++x) {
            const QColor currentColor = QColor::fromRgba(currentLine[x]);
            const QColor baselineColor = QColor::fromRgba(baselineLine[x]);
            if (!visuallyDifferent(currentColor, baselineColor, channelThreshold)) {
                diffLine[x] = qRgba(0, 0, 0, 0);
                continue;
            }
            ++result.changedPixels;
            diffLine[x] = qRgba(255, 64, 64, 220);
        }
    }

    const qint64 totalPixels = static_cast<qint64>(current.width()) * current.height();
    result.changedRatio = totalPixels > 0
        ? static_cast<double>(result.changedPixels) / static_cast<double>(totalPixels)
        : 0.0;
    result.diffImage = diff;

    constexpr double maximumChangedRatio = 0.01;
    result.ok = result.changedRatio <= maximumChangedRatio;
    if (result.ok) {
        result.message = QStringLiteral("Visual baseline matched: %1 (%2% changed)")
                             .arg(name)
                             .arg(result.changedRatio * 100.0, 0, 'f', 3);
        return result;
    }

    if (!QDir().mkpath(outputDirectory) || !diff.save(diffPath)) {
        result.message = comparisonMessage(name,
                                           result.changedPixels,
                                           result.changedRatio,
                                           actualPath,
                                           QStringLiteral("<diff save failed>"));
        return result;
    }
    result.message = comparisonMessage(name,
                                       result.changedPixels,
                                       result.changedRatio,
                                       actualPath,
                                       diffPath);
    return result;
}


