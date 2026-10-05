// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.
#include "WindowOperations.h"
namespace IndependentDesktops
{
    namespace
    {
        LRESULT CALLBACK ShutdownProcedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
        {
            if (message == WM_NCCREATE)
            {
                const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lparam);
                SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
            }
            if (message == WM_CLOSE)
            {
                const auto stop = reinterpret_cast<HANDLE>(GetWindowLongPtrW(window, GWLP_USERDATA));
                if (stop)
                    SetEvent(stop);
                return 0;
            }
            return DefWindowProcW(window, message, wparam, lparam);
        }
    }
    ShutdownWindow::ShutdownWindow(HANDLE stopEvent)
    {
        const auto instance = GetModuleHandleW(nullptr);
        constexpr wchar_t name[] = L"PowerToys.IndependentDesktops.Shutdown.6EAF3B79";
        WNDCLASSW type{};
        type.lpfnWndProc = ShutdownProcedure;
        type.hInstance = instance;
        type.lpszClassName = name;
        if (!stopEvent || (!RegisterClassW(&type) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS))
            return;
        m_window = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, name, L"", WS_POPUP, 0, 0, 0, 0, nullptr, nullptr, instance, stopEvent);
    }
    ShutdownWindow::~ShutdownWindow()
    {
        if (m_window)
            DestroyWindow(m_window);
    }
    bool RestoreTaggedWindows()
    {
        struct Intent
        {
            WindowId id;
            int show;
        };
        struct Context
        {
            std::vector<Intent> windows;
            bool complete{ true };
        } context;
        const BOOL enumerated = EnumWindows([](HWND window, LPARAM data) -> BOOL {
            auto& context = *reinterpret_cast<Context*>(data);
            try
            {
                const auto id = IdentifyWindow(window);
                if (!id || id->incarnation == 0)
                    return TRUE;
                const auto command = reinterpret_cast<ULONG_PTR>(GetPropW(window, RestoreIntentProperty));
                if (command != 0 && command != SW_SHOWNA && command != SW_SHOWMINNOACTIVE)
                {
                    context.complete = false;
                    return FALSE;
                }
                context.windows.push_back({ *id, static_cast<int>(command) });
                return TRUE;
            }
            catch (...)
            {
                context.complete = false;
                return FALSE;
            }
        },
                                            reinterpret_cast<LPARAM>(&context));
        if (!enumerated || !context.complete)
            return false;
        bool restored = true;
        for (const auto& intent : context.windows)
            if (intent.show && MatchesWindow(intent.id) && !ShowWindowAsync(reinterpret_cast<HWND>(static_cast<ULONG_PTR>(intent.id.handle)), intent.show))
                restored = false;
        const ULONGLONG deadline = GetTickCount64() + 2000;
        for (const auto& intent : context.windows)
        {
            if (!MatchesWindow(intent.id))
                continue; // A prior watchdog may already have completed recovery.
            const auto now = GetTickCount64();
            if (intent.show && !WaitForVisibility(intent.id, true, now < deadline ? static_cast<DWORD>(deadline - now) : 0))
            {
                if (MatchesWindow(intent.id))
                    restored = false;
                continue;
            }
            if (MatchesWindow(intent.id))
            {
                const HWND window = reinterpret_cast<HWND>(static_cast<ULONG_PTR>(intent.id.handle));
                RemovePropW(window, RestoreIntentProperty);
                RemovePropW(window, RecoveryProperty);
            }
        }
        return restored;
    }
    std::optional<WindowId> IdentifyWindow(HWND window)
    {
        if (!IsWindow(window))
            return {};
        DWORD processId{};
        const DWORD threadId = GetWindowThreadProcessId(window, &processId);
        UniqueHandle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId));
        FILETIME created{}, exited{}, kernel{}, user{};
        if (!threadId || !process || !GetProcessTimes(process.get(), &created, &exited, &kernel, &user))
            return {};
        const auto timestamp = (static_cast<std::uint64_t>(created.dwHighDateTime) << 32) | created.dwLowDateTime;
        return WindowId{ reinterpret_cast<ULONG_PTR>(window), processId, threadId, timestamp, reinterpret_cast<ULONG_PTR>(GetPropW(window, RecoveryProperty)) };
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
            if (!MatchesWindow(id))
                return false;
            if ((IsWindowVisible(reinterpret_cast<HWND>(static_cast<ULONG_PTR>(id.handle))) != FALSE) == visible)
                return true;
            if (GetTickCount64() >= deadline)
                break;
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
        if (!monitor || !GetMonitorInfoW(monitor, &info))
            return {};
        return info.szDevice;
    }
}
