#pragma once

#include "workspace/native_workspace_types.h"

class QSettings;

class NativeWorkspaceStore final {
public:
    explicit NativeWorkspaceStore(QSettings *settings);

    NativeWorkspaceSnapshot load() const;
    bool save(const NativeWorkspaceSnapshot &snapshot);

private:
    QSettings *settings_ = nullptr;
};
