#include <QCoreApplication>
#include <QDir>
#include <QtEndian>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest/QtTest>

class NativeSelfTestTest final : public QObject {
    Q_OBJECT

private slots:
    void quickSelfTestReportsRendererReadiness();
    void exercisesMediaLifecycle();
    void isBuiltAsWindowsGuiExecutable();
};

quint16 peSubsystem(const QString &executable)
{
    QFile file(executable);
    if (!file.open(QIODevice::ReadOnly)) return 0;
    if (!file.seek(0x3c)) return 0;
    const QByteArray offsetBytes = file.read(4);
    if (offsetBytes.size() != 4) return 0;
    const quint32 peOffset = qFromLittleEndian<quint32>(
        reinterpret_cast<const uchar *>(offsetBytes.constData()));
    if (!file.seek(static_cast<qint64>(peOffset) + 4 + 20 + 68)) return 0;
    const QByteArray subsystemBytes = file.read(2);
    if (subsystemBytes.size() != 2) return 0;
    return qFromLittleEndian<quint16>(
        reinterpret_cast<const uchar *>(subsystemBytes.constData()));
}

void NativeSelfTestTest::quickSelfTestReportsRendererReadiness()
{
    const QString executable = QDir(QCoreApplication::applicationDirPath())
        .filePath(QStringLiteral("douyu_monitor_native.exe"));
    QVERIFY(QFileInfo::exists(executable));

    QProcess process;
    process.setProgram(executable);
    process.setArguments({QStringLiteral("--self-test")});
    auto environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("windows"));
    process.setProcessEnvironment(environment);
    process.start();
    QVERIFY2(process.waitForFinished(60000), qPrintable(process.errorString()));
    QCOMPARE(process.exitStatus(), QProcess::NormalExit);
    QCOMPARE(process.exitCode(), 0);

    const QByteArray output = process.readAllStandardOutput() + process.readAllStandardError();
    QVERIFY(output.contains("native self-test passed: Qt Quick renderer"));
    QVERIFY(!output.contains(QByteArrayLiteral("Q") + QByteArrayLiteral("Widget")));
}

void NativeSelfTestTest::exercisesMediaLifecycle()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    QFile fixture(temporaryDirectory.filePath(QStringLiteral("self-test.y4m")));
    QVERIFY(fixture.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QByteArray y4m("YUV4MPEG2 W2 H2 F25:1 Ip A1:1 C444");
    y4m.append(static_cast<char>(10));
    const QByteArray redFrame("RRRRZZZZ\xF0\xF0\xF0\xF0", 12);
    for (int frame = 0; frame < 150; ++frame) {
        y4m.append("FRAME");
        y4m.append(static_cast<char>(10));
        y4m.append(redFrame);
    }
    QCOMPARE(fixture.write(y4m), static_cast<qint64>(y4m.size()));
    fixture.close();

    const QString executable = QDir(QCoreApplication::applicationDirPath())
        .filePath(QStringLiteral("douyu_monitor_native.exe"));
    QVERIFY(QFileInfo::exists(executable));

    QProcess process;
    process.setProgram(executable);
    process.setArguments({
        QStringLiteral("--self-test"),
        QStringLiteral("--media"),
        fixture.fileName(),
    });
    process.start();
    QVERIFY2(process.waitForFinished(60000), qPrintable(process.errorString()));
    QCOMPARE(process.exitStatus(), QProcess::NormalExit);
    QCOMPARE(process.exitCode(), 0);

    const QByteArray output = process.readAllStandardOutput() + process.readAllStandardError();
    QVERIFY(output.contains("load"));
    QVERIFY(output.contains("first-frame"));
    QVERIFY(output.contains("stop"));
    QVERIFY(output.contains("release"));
    QVERIFY(!output.contains(fixture.fileName().toUtf8()));
}

void NativeSelfTestTest::isBuiltAsWindowsGuiExecutable()
{
    const QString executable = QDir(QCoreApplication::applicationDirPath())
        .filePath(QStringLiteral("douyu_monitor_native.exe"));
    QCOMPARE(peSubsystem(executable), quint16(2));
}

QTEST_GUILESS_MAIN(NativeSelfTestTest)

#include "native_self_test_test.moc"
