// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.
#include "WindowOperations.h"
namespace IndependentDesktops
{
    std::optional<WindowId> IdentifyWindow(HWND window)
    {
        if (!IsWindow(window)) return {};
        DWORD processId{};
        const DWORD threadId = GetWindowThreadProcessId(window, &processId);
        UniqueHandle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId));
        FILETIME created{}, exited{}, kernel{}, user{};
        if (!threadId || !process || !GetProcessTimes(process.get(), &created, &exited, &kernel, &user)) return {};
        const auto timestamp = (static_cast<std::uint64_t>(created.dwHighDateTime) << 32) | created.dwLowDateTime;
        return WindowId{ reinterpret_cast<ULONG_PTR>(window), processId, threadId, timestamp,
                         reinterpret_cast<ULONG_PTR>(GetPropW(window, RecoveryProperty)) };
    }
    bool MatchesWindow(const WindowId& id, bool requireMarker)
    {
        const auto current = IdentifyWindow(reinterpret_cast<HWND>(static_cast<ULONG_PTR>(id.handle)));
        return current && current->process == id.process && current->thread == id.thread && current->created == id.created &&
               (!requireMarker || (id.incarnation != 0 && current->incarnation == id.incarnation));
    }
    bool WaitForVisibility(const WindowId& id, bool visible, DWORD timeoutMs)
    {
        const ULONGLONG deadline = GetTickCount64() + timeoutMs;
        do
        {
            if (!MatchesWindow(id)) return false;
            if ((IsWindowVisible(reinterpret_cast<HWND>(static_cast<ULONG_PTR>(id.handle))) != FALSE) == visible) return true;
            if (GetTickCount64() >= deadline) break;
            // ShowWindowAsync queues work even for a window on this thread. Only
            // dispatch that window's messages; never re-enter the manager's queue.
            if (id.thread == GetCurrentThreadId())
            {
                MSG message{};
                const HWND window = reinterpret_cast<HWND>(static_cast<ULONG_PTR>(id.handle));
                while (PeekMessageW(&message, window, 0, 0, PM_REMOVE))
                {
                    TranslateMessage(&message);
                    DispatchMessageW(&message);
                }
            }
            Sleep(10);
        } while (true);
        return false;
    }
    std::wstring MonitorName(HMONITOR monitor)
    {
        MONITORINFOEXW info{};
        info.cbSize = sizeof(info);
        if (!monitor || !GetMonitorInfoW(monitor, &info)) return {};
        return info.szDevice;
    }
}
