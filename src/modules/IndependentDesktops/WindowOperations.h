// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.
#pragma once
#include "WorkspaceModel.h"
#include <windows.h>

namespace IndependentDesktops
{
    inline constexpr wchar_t RecoveryProperty[] = L"PowerToys.IndependentDesktops.Recovery.6EAF3B79";
    std::optional<WindowId> IdentifyWindow(HWND window);
    bool MatchesWindow(const WindowId& id, bool requireMarker = true);
    bool WaitForVisibility(const WindowId& id, bool visible, DWORD timeoutMs = 1000);
    std::wstring MonitorName(HMONITOR monitor);
    class UniqueHandle
    {
        HANDLE m_handle{};
    public:
        UniqueHandle() = default;
        explicit UniqueHandle(HANDLE handle) : m_handle(handle) {}
        ~UniqueHandle() { if (m_handle && m_handle != INVALID_HANDLE_VALUE) CloseHandle(m_handle); }
        UniqueHandle(const UniqueHandle&) = delete;
        UniqueHandle& operator=(const UniqueHandle&) = delete;
        UniqueHandle(UniqueHandle&& other) noexcept : m_handle(other.release()) {}
        UniqueHandle& operator=(UniqueHandle&& other) noexcept { reset(other.release()); return *this; }
        HANDLE get() const { return m_handle; }
        HANDLE release() { HANDLE value = m_handle; m_handle = nullptr; return value; }
        void reset(HANDLE value = nullptr) { if (m_handle && m_handle != INVALID_HANDLE_VALUE) CloseHandle(m_handle); m_handle = value; }
        explicit operator bool() const { return m_handle && m_handle != INVALID_HANDLE_VALUE; }
    };
}
