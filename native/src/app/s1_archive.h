#pragma once

#include <QByteArray>
#include <QString>
#include <QVariantMap>

class S1Archive final {
public:
    static QVariantMap parse(const QByteArray &json, QString *error = nullptr);
    static QVariantMap bundled();
};
