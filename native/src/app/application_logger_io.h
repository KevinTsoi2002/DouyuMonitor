#pragma once

#include <QByteArray>
#include <QString>

#include <memory>

class ApplicationLoggerBackend {
public:
    virtual ~ApplicationLoggerBackend() = default;

    virtual bool ensureDirectory(const QString &directory) = 0;
    virtual bool open(const QString &path) = 0;
    virtual qint64 write(const QByteArray &data) = 0;
    virtual bool flush() = 0;
    virtual bool close() = 0;
};

