#pragma once

#include <QImage>
#include <QRect>
#include <QRectF>
#include <QString>
#include <QVariantList>
#include <QVector>

class QQuickItem;
class QQuickWindow;

struct VisualComparisonResult {
    bool ok = false;
    int changedPixels = 0;
    double changedRatio = 0.0;
    QString message;
    QImage diffImage;
};

QQuickItem *visualItemByObjectName(QQuickItem *root, const QString &objectName);
QRectF visualSceneRect(QQuickItem *item);
bool visualRectsOverlap(const QRectF &first, const QRectF &second, qreal tolerance = 0.5);

QVariantList visualRoomFixtures(int count,
                                bool multiAudio = false,
                                bool includeQualities = false);

QImage visualCapture(QQuickWindow *window, const QString &name);
VisualComparisonResult visualCompareWithBaseline(
    const QImage &image,
    const QString &name,
    const QVector<QRect> &ignoredRegions = {});

