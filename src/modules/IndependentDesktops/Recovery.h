// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.
#pragma once
#include "WindowOperations.h"
#include <memory>

namespace IndependentDesktops
{
    class RecoverySession
    {
        struct State;
        std::unique_ptr<State> m_state;
    public:
        RecoverySession();
        ~RecoverySession();
        RecoverySession(const RecoverySession&) = delete;
        RecoverySession& operator=(const RecoverySession&) = delete;
        bool Start(const std::wstring& executable);
        bool Healthy() const;
        HANDLE ProcessHandle() const;
        std::optional<WindowId> Register(HWND window);
        std::optional<WindowId> Find(HWND window) const;
        void Prune();
        bool Hide(const WindowId& id);
        bool Show(const WindowId& id);
        bool RestoreAll();
        void UnregisterAll();
    };
    int RunRecovery(HANDLE mapping, HANDLE ready, HANDLE parent);
    bool ParseHandle(const wchar_t* text, HANDLE& handle);
}
