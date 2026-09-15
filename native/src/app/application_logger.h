#pragma once

#include <QString>

class ApplicationLogger final {
public:
    static void install(const QString &directory = {});
    static void flush();
    static QString currentLogPath();
    static void resetForTest();
};
