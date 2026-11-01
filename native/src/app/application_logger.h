#pragma once

#include <QString>

#include <functional>
#include <memory>

class ApplicationLogger final {
public:
    // File logging remains optional; callers can inspect the result without terminating the app.
    enum class State {
        Uninstalled,
        Ready,
        Unavailable,
        Closed,
    };

    enum class Error {
        None,
        DirectoryCreateFailed,
        OpenFailed,
        WriteFailed,
        PartialWrite,
        FlushFailed,
        CloseFailed,
    };

    static bool install(const QString &directory = {});
    static bool flush();
    static bool shutdown();
    static QString currentLogPath();
    static State state();
    static Error lastError();
    static void resetForTest();

#ifdef DOUYU_APPLICATION_LOGGER_TESTING
    static void setTestBackendFactory(std::function<std::unique_ptr<class ApplicationLoggerBackend>()> factory);
    static void clearTestBackendFactory();
#endif
};
