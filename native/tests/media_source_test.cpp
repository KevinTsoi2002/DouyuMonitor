#include <QtTest/QtTest>

#include "media/media_source.h"
#include "service/stream_service_protocol.h"

class MediaSourceTest final : public QObject {
    Q_OBJECT

private slots:
    void acceptsLocalPathDescriptor();
    void rejectsEmptyDescriptor();
    void rejectsUnsupportedSchemes();
    void keepsStableDescriptionFreeOfSensitiveParts();
    void acceptsAllowedRemoteVariant();
    void rejectsUnsafeRemoteVariants();
    void keepsRemoteDescriptionFreeOfUrlData();
};

void MediaSourceTest::acceptsLocalPathDescriptor()
{
    const auto source = MediaSource::fromDescriptor(QStringLiteral("C:/media/clip.ppm"));

    QVERIFY(source.has_value());
    QCOMPARE(source->kind(), MediaSource::Kind::LocalFile);
    QCOMPARE(source->localPath(), QStringLiteral("C:/media/clip.ppm"));
}

void MediaSourceTest::rejectsEmptyDescriptor()
{
    QVERIFY(!MediaSource::fromDescriptor(QString()).has_value());
    QVERIFY(!MediaSource::fromDescriptor(QStringLiteral("   ")).has_value());
}

void MediaSourceTest::rejectsUnsupportedSchemes()
{
    QVERIFY(!MediaSource::fromDescriptor(QStringLiteral("https://example.invalid/live"))
                 .has_value());
    QVERIFY(!MediaSource::fromDescriptor(QStringLiteral("rtmp://example.invalid/live"))
                 .has_value());
    QVERIFY(!MediaSource::fromDescriptor(QStringLiteral("mailto:someone@example.invalid"))
                 .has_value());
}

void MediaSourceTest::keepsStableDescriptionFreeOfSensitiveParts()
{
    const auto source = MediaSource::fromDescriptor(
        QStringLiteral("C:/media/private clip.ppm?token=secret#fragment"));

    QVERIFY(source.has_value());
    const QString description = source->stableDescription();
    QVERIFY(!description.contains(QLatin1Char('?')));
    QVERIFY(!description.contains(QLatin1Char('#')));
    QVERIFY(!description.contains(QLatin1Char('@')));
    QCOMPARE(description, QStringLiteral("local-file"));
}

void MediaSourceTest::acceptsAllowedRemoteVariant()
{
    StreamVariant variant{
        QStringLiteral("flv-auto"),
        QStringLiteral("Auto"),
        StreamQuality::Auto,
        QStringLiteral("flv"),
        QUrl(QStringLiteral("https://live.douyucdn.cn/live/test.flv?wsAuth=redacted")),
    };

    const auto source = MediaSource::fromRemoteVariant(QStringLiteral("63136"), variant);

    QVERIFY(source.has_value());
    QCOMPARE(source->kind(), MediaSource::Kind::RemoteStream);
    QCOMPARE(source->roomId(), QStringLiteral("63136"));
    QCOMPARE(source->variantId(), QStringLiteral("flv-auto"));
    QCOMPARE(source->quality(), StreamQuality::Auto);
    QCOMPARE(source->container(), QStringLiteral("flv"));
    QCOMPARE(source->remoteUrl(), variant.playbackUrl);
    QCOMPARE(source->stableDescription(), QStringLiteral("remote-stream"));
}

void MediaSourceTest::rejectsUnsafeRemoteVariants()
{
    const auto makeVariant = [](const QString &id,
                                const QString &container,
                                const QUrl &url) {
        return StreamVariant{id, QStringLiteral("Auto"), StreamQuality::Auto, container, url};
    };

    const QUrl allowed(QStringLiteral("https://live.douyucdn.cn/live/test.flv"));
    QVERIFY(!MediaSource::fromRemoteVariant(QStringLiteral("63136"),
                                             makeVariant(QStringLiteral("flv-auto"), QStringLiteral("flv"),
                                                         QUrl(QStringLiteral("ftp://live.douyucdn.cn/live/test.flv"))))
                 .has_value());
    QVERIFY(!MediaSource::fromRemoteVariant(QStringLiteral("63136"),
                                             makeVariant(QStringLiteral("flv-auto"), QStringLiteral("flv"),
                                                         QUrl(QStringLiteral("https://user:secret@live.douyucdn.cn/x.flv"))))
                 .has_value());
    QVERIFY(!MediaSource::fromRemoteVariant(QStringLiteral("63136"),
                                             makeVariant(QStringLiteral("flv-auto"), QStringLiteral("flv"),
                                                         QUrl(QStringLiteral("https://example.invalid/x.flv"))))
                 .has_value());
    QVERIFY(!MediaSource::fromRemoteVariant(QStringLiteral("not-a-room"),
                                             makeVariant(QStringLiteral("flv-auto"), QStringLiteral("flv"), allowed))
                 .has_value());
    QVERIFY(!MediaSource::fromRemoteVariant(QStringLiteral("63136"),
                                             makeVariant(QString(), QStringLiteral("flv"), allowed))
                 .has_value());
    QVERIFY(!MediaSource::fromRemoteVariant(QStringLiteral("63136"),
                                             makeVariant(QStringLiteral("flv-auto"), QString(), allowed))
                 .has_value());
    QVERIFY(!MediaSource::fromRemoteVariant(QStringLiteral("63136"),
                                             makeVariant(QStringLiteral("flv-auto"), QStringLiteral("flv"), QUrl()))
                 .has_value());
}

void MediaSourceTest::keepsRemoteDescriptionFreeOfUrlData()
{
    const StreamVariant variant{
        QStringLiteral("flv-auto"),
        QStringLiteral("Auto"),
        StreamQuality::Auto,
        QStringLiteral("flv"),
        QUrl(QStringLiteral("https://live.douyucdn.cn/live/test.flv?wsAuth=redacted")),
    };
    const auto source = MediaSource::fromRemoteVariant(QStringLiteral("63136"), variant);

    QVERIFY(source.has_value());
    const QString description = source->stableDescription();
    QVERIFY(!description.contains(QLatin1Char('?')));
    QVERIFY(!description.contains(QLatin1Char('#')));
    QVERIFY(!description.contains(QLatin1Char('@')));
    QVERIFY(!description.contains(QStringLiteral("token"), Qt::CaseInsensitive));
    QVERIFY(!description.contains(QStringLiteral("wsAuth"), Qt::CaseInsensitive));
}

QTEST_MAIN(MediaSourceTest)

#include "media_source_test.moc"
