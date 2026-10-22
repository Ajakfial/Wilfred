#pragma once

#include <windows.h>

#include <functional>
#include <string>

enum class MsiEventKind {
    Progress,
    Status,
};

struct MsiEvent {
    MsiEventKind kind = MsiEventKind::Status;
    int percent = 0;      // only meaningful for Progress
    std::wstring text;    // current action / status line
};

struct MsiResult {
    UINT code = ERROR_SUCCESS;
    std::wstring error;
};

// True when the file starts with the OLE compound-file signature that all
// Windows Installer packages use. Cheap guard against a truncated resource.
bool LooksLikeMsi(const std::wstring& path);

// Runs MsiInstallProductW with an external UI handler that forwards progress
// to `sink`. Must be called from a single thread; the sink is invoked on that
// same thread. Blocks until the installation completes.
MsiResult RunMsiInstall(const std::wstring& msiPath, const std::wstring& logPath,
                        const std::function<void(const MsiEvent&)>& sink);
