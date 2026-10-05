// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.
#pragma once
#include "WorkspaceModel.h"
#include <windows.h>

namespace IndependentDesktops
{
    inline constexpr wchar_t RecoveryProperty[] = L"PowerToys.IndependentDesktops.Recovery.6EAF3B79";
    inline constexpr wchar_t RestoreIntentProperty[] = L"PowerToys.IndependentDesktops.RestoreIntent.6EAF3B79";
    // Caller must exclusively own ManagerMutex; this never runs alongside a live manager.
    bool RestoreTaggedWindows();
    std::optional<WindowId> IdentifyWindow(HWND window);
    bool MatchesWindow(const WindowId& id, bool requireMarker = true);
    bool WaitForVisibility(const WindowId& id, bool visible, DWORD timeoutMs = 1000);
    std::wstring MonitorName(HMONITOR monitor);
    // A hidden top-level window lets the installer request graceful restoration.
    // The recovery process owns no such window and must never be forcibly terminated.
    class ShutdownWindow
    {
        HWND m_window{};

    public:
        explicit ShutdownWindow(HANDLE stopEvent);
        ~ShutdownWindow();
        ShutdownWindow(const ShutdownWindow&) = delete;
        ShutdownWindow& operator=(const ShutdownWindow&) = delete;
        HWND get() const { return m_window; }
        explicit operator bool() const { return m_window != nullptr; }
    };
    class UniqueHandle
    {
        HANDLE m_handle{};

    public:
        UniqueHandle() = default;
        explicit UniqueHandle(HANDLE handle) :
            m_handle(handle) {}
        ~UniqueHandle()
        {
            if (m_handle && m_handle != INVALID_HANDLE_VALUE)
                CloseHandle(m_handle);
        }
        UniqueHandle(const UniqueHandle&) = delete;
        UniqueHandle& operator=(const UniqueHandle&) = delete;
        UniqueHandle(UniqueHandle&& other) noexcept :
            m_handle(other.release()) {}
        UniqueHandle& operator=(UniqueHandle&& other) noexcept
        {
            reset(other.release());
            return *this;
        }
        HANDLE get() const { return m_handle; }
        HANDLE release()
        {
            HANDLE value = m_handle;
            m_handle = nullptr;
            return value;
        }
        void reset(HANDLE value = nullptr)
        {
            if (m_handle && m_handle != INVALID_HANDLE_VALUE)
                CloseHandle(m_handle);
            m_handle = value;
        }
        explicit operator bool() const { return m_handle && m_handle != INVALID_HANDLE_VALUE; }
    };
}
