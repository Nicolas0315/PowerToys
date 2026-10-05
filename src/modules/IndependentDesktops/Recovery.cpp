// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.
#include "Recovery.h"
namespace IndependentDesktops
{
    struct RecoverySession::State {};
    RecoverySession::RecoverySession() : m_state(std::make_unique<State>()) {}
    RecoverySession::~RecoverySession() = default;
    bool RecoverySession::Start(const std::wstring&) { return false; }
    bool RecoverySession::Healthy() const { return false; }
    HANDLE RecoverySession::ProcessHandle() const { return nullptr; }
    std::optional<WindowId> RecoverySession::Register(HWND) { return {}; }
    std::optional<WindowId> RecoverySession::Find(HWND) const { return {}; }
    void RecoverySession::Prune() {}
    bool RecoverySession::Hide(const WindowId&) { return false; }
    bool RecoverySession::Show(const WindowId&) { return false; }
    bool RecoverySession::RestoreAll() { return true; }
    void RecoverySession::UnregisterAll() {}
    int RunRecovery(HANDLE, HANDLE, HANDLE) { return 1; }
    bool ParseHandle(const wchar_t*, HANDLE&) { return false; }
}
