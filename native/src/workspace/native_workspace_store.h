#pragma once

#include "workspace/native_workspace_types.h"

class QSettings;

enum class NativeWorkspaceLoadStatus {
    Loaded,
    Missing,
    Invalid,
    UnsupportedVersion,
    ReadError,
};

struct NativeWorkspaceLoadResult {
    NativeWorkspaceLoadStatus status = NativeWorkspaceLoadStatus::ReadError;
    NativeWorkspaceSnapshot snapshot;
};

class NativeWorkspaceStore final {
public:
    explicit NativeWorkspaceStore(QSettings *settings);

    NativeWorkspaceLoadResult loadResult() const;
    NativeWorkspaceSnapshot load() const;
    bool save(const NativeWorkspaceSnapshot &snapshot);

private:
    QSettings *settings_ = nullptr;
};
