#pragma once

#include <QDir>
#include <QFile>
#include <QFont>
#include <QFontDatabase>
#include <QGuiApplication>

namespace DouyuTestSupport {

inline bool prepareTestFonts()
{
#ifdef Q_OS_WIN
    // Set this before the first font database query. The offscreen plugin
    // otherwise searches the build directory's lib/fonts instead of Windows.
    if (QGuiApplication::platformName() == QStringLiteral("offscreen")
        && qEnvironmentVariableIsEmpty("QT_QPA_FONTDIR")) {
        const QString windowsDirectory = qEnvironmentVariable("WINDIR");
        if (windowsDirectory.isEmpty()) return false;
        const QString fontDirectory = QDir(windowsDirectory).filePath(QStringLiteral("Fonts"));
        if (!QDir(fontDirectory).exists()) return false;
        if (!qputenv("QT_QPA_FONTDIR", QFile::encodeName(fontDirectory))) return false;
    }
#endif
    const QStringList chineseFamilies = QFontDatabase::families(QFontDatabase::SimplifiedChinese);
    if (chineseFamilies.isEmpty()) return false;
#ifdef Q_OS_WIN
    if (QGuiApplication::platformName() == QStringLiteral("offscreen")) {
        QFont font = QGuiApplication::font();
        // Generic "Sans Serif" can resolve to an arbitrary installed font.
        // Keep the original size and weight and use a Windows UI family.
        const QString uiFamily = QStringLiteral("Microsoft YaHei UI");
        const QString fallbackFamily = QStringLiteral("Microsoft YaHei");
        font.setFamily(QFontDatabase::hasFamily(uiFamily) ? uiFamily
                       : QFontDatabase::hasFamily(fallbackFamily) ? fallbackFamily
                       : chineseFamilies.constFirst());
        QGuiApplication::setFont(font);
    }
#endif
    return true;
}

} // namespace DouyuTestSupport
