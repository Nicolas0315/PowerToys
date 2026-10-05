// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.
#include "Recovery.h"
#include "resource.h"
#include <dwmapi.h>
#include <shellapi.h>
#include <shobjidl.h>
#include <wrl/client.h>
#include <array>
#include <set>

using namespace IndependentDesktops;

namespace
{
    std::wstring ExecutablePath()
    {
        std::wstring value(32768, L'\0');
        const DWORD length = GetModuleFileNameW(nullptr, value.data(), static_cast<DWORD>(value.size()));
        if (!length || length >= value.size())
            return {};
        value.resize(length);
        return value;
    }
    std::wstring DesktopKey(const GUID& id)
    {
        if (IsEqualGUID(id, GUID_NULL))
            return {};
        wchar_t value[40]{};
        return StringFromGUID2(id, value, 40) ? value : L"";
    }
    std::wstring CurrentDesktop(IVirtualDesktopManager* manager)
    {
        DWORD session{};
        ProcessIdToSessionId(GetCurrentProcessId(), &session);
        const std::wstring paths[] = {
            L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\VirtualDesktops",
            L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\SessionInfo\\" + std::to_wstring(session) + L"\\VirtualDesktops"
        };
        for (const auto& path : paths)
        {
            GUID id{};
            DWORD bytes = sizeof(id);
            if (RegGetValueW(HKEY_CURRENT_USER, path.c_str(), L"CurrentVirtualDesktop", RRF_RT_REG_BINARY, nullptr, &id, &bytes) == ERROR_SUCCESS && bytes == sizeof(id))
                if (auto result = DesktopKey(id); !result.empty())
                    return result;
        }
        // A foreground pinned window may report the generic AllDesktops view.
        // Only use this fallback when its ID belongs to Explorer's real desktop list.
        const HWND foreground = GetForegroundWindow();
        BOOL current{};
        GUID id{};
        if (foreground && SUCCEEDED(manager->IsWindowOnCurrentVirtualDesktop(foreground, &current)) && current && SUCCEEDED(manager->GetWindowDesktopId(foreground, &id)))
        {
            for (const auto& path : paths)
            {
                std::vector<GUID> desktops(1024);
                DWORD bytes = static_cast<DWORD>(desktops.size() * sizeof(GUID));
                if (RegGetValueW(HKEY_CURRENT_USER, path.c_str(), L"VirtualDesktopIDs", RRF_RT_REG_BINARY, nullptr, desktops.data(), &bytes) != ERROR_SUCCESS || bytes % sizeof(GUID) != 0)
                    continue;
                for (std::size_t i = 0; i < bytes / sizeof(GUID); ++i)
                    if (IsEqualGUID(id, desktops[i]))
                        return DesktopKey(id);
            }
        }
        return {};
    }
    bool ShellWindow(HWND window)
    {
        if (window == GetShellWindow() || window == GetDesktopWindow())
            return true;
        wchar_t name[256]{};
        GetClassNameW(window, name, 256);
        const wchar_t* excluded[] = { L"Progman", L"WorkerW", L"Shell_TrayWnd", L"Shell_SecondaryTrayWnd", L"MultitaskingViewFrame", L"#32768", L"tooltips_class32" };
        for (const auto* value : excluded)
            if (std::wstring(name) == value)
                return true;
        return false;
    }
    bool PowerToysWindow(const WindowId& id)
    {
        UniqueHandle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, id.process));
        wchar_t path[32768]{};
        DWORD size = 32768;
        if (!process || !QueryFullProcessImageNameW(process.get(), 0, path, &size))
            return true;
        const std::wstring full(path, size);
        const auto name = full.substr(full.find_last_of(L"\\/") + 1);
        return _wcsnicmp(name.c_str(), L"PowerToys.", 10) == 0 || _wcsicmp(name.c_str(), L"PowerToys.exe") == 0;
    }
    std::set<std::wstring> ConnectedMonitors()
    {
        std::set<std::wstring> values;
        EnumDisplayMonitors(nullptr, nullptr, [](HMONITOR monitor, HDC, LPRECT, LPARAM data) -> BOOL {
            try
            {
                auto& monitors = *reinterpret_cast<std::set<std::wstring>*>(data);
                auto name = MonitorName(monitor);
                if (!name.empty()) monitors.insert(std::move(name));
                return TRUE;
            }
            catch (...) { return FALSE; } }, reinterpret_cast<LPARAM>(&values));
        return values;
    }
    std::wstring Resource(unsigned id)
    {
        wchar_t text[512]{};
        const int length = LoadStringW(GetModuleHandleW(nullptr), id, text, 512);
        return length > 0 ? std::wstring(text, static_cast<std::size_t>(length)) : L"";
    }
    class Manager
    {
        RecoverySession m_recovery;
        WorkspaceModel m_model;
        Microsoft::WRL::ComPtr<IVirtualDesktopManager> m_desktops;
        std::wstring m_desktop;
        std::set<std::wstring> m_monitors;
        DWORD m_runnerPid{};
        HWND m_indicator{};
        ULONGLONG m_indicatorUntil{};
        bool m_failed{};

        bool Eligible(HWND window, const WindowId& id, bool root) const
        {
            if (id.process == GetCurrentProcessId() || id.process == m_runnerPid || ShellWindow(window) || PowerToysWindow(id))
                return false;
            const auto style = GetWindowLongPtrW(window, GWL_STYLE);
            const auto extended = GetWindowLongPtrW(window, GWL_EXSTYLE);
            if ((style & WS_CHILD) || (root && (extended & WS_EX_TOOLWINDOW)))
                return false;
            DWORD cloaked{};
            if (FAILED(DwmGetWindowAttribute(window, DWMWA_CLOAKED, &cloaked, sizeof(cloaked))) || cloaked)
                return false;
            BOOL onCurrent{};
            return SUCCEEDED(m_desktops->IsWindowOnCurrentVirtualDesktop(window, &onCurrent)) && onCurrent;
        }
        std::optional<WindowId> Track(HWND window, bool root)
        {
            if (auto id = m_recovery.Find(window))
                return id;
            auto id = IdentifyWindow(window);
            if (!id || !IsWindowVisible(window) || !Eligible(window, *id, root))
                return {};
            GUID desktop{};
            if (FAILED(m_desktops->GetWindowDesktopId(window, &desktop)) || DesktopKey(desktop) != m_desktop)
                return {}; // Excludes the generic AllDesktops view and other reported desktops.
            auto registered = m_recovery.Register(window);
            if (!registered && m_recovery.RegisteredCount() >= MaxTrackedWindows)
                m_failed = true;
            return registered;
        }
        std::vector<WindowSnapshot> Collect()
        {
            struct Context
            {
                Manager* manager;
                std::vector<WindowSnapshot> windows;
                bool complete{ true };
            } context{ this, {}, true };
            const BOOL enumerated = EnumWindows([](HWND window, LPARAM data) -> BOOL {
                auto& context = *reinterpret_cast<Context*>(data);
                try
                {
                    auto& self = *context.manager;
                    const HWND root = GetAncestor(window, GA_ROOTOWNER);
                    if (!root)
                        return TRUE;
                    const auto rootId = self.Track(root, true);
                    if (!rootId)
                        return TRUE;
                    const auto id = self.Track(window, window == root);
                    if (!id)
                        return TRUE;
                    GUID desktop{};
                    if (FAILED(self.m_desktops->GetWindowDesktopId(window, &desktop)))
                    {
                        self.m_failed = true;
                        return FALSE;
                    }
                    context.windows.push_back({ *id, *rootId, MonitorName(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST)), DesktopKey(desktop), IsWindowVisible(window) != FALSE, true });
                    return !self.m_failed;
                }
                catch (...)
                {
                    context.complete = false;
                    return FALSE;
                }
            },
                                                reinterpret_cast<LPARAM>(&context));
            if (!enumerated || !context.complete)
                m_failed = true;
            return context.windows;
        }
        void Notify(const std::wstring& monitor, const std::wstring& text)
        {
            if (m_indicator)
            {
                DestroyWindow(m_indicator);
                m_indicator = nullptr;
            }
            struct Locate
            {
                std::wstring wanted;
                HMONITOR result{};
            } locate{ monitor, nullptr };
            EnumDisplayMonitors(nullptr, nullptr, [](HMONITOR handle, HDC, LPRECT, LPARAM data) -> BOOL {
                auto& value = *reinterpret_cast<Locate*>(data);
                if (MonitorName(handle) == value.wanted) { value.result = handle; return FALSE; }
                return TRUE; }, reinterpret_cast<LPARAM>(&locate));
            MONITORINFO info{ sizeof(info), {}, {}, 0 };
            if (!locate.result || !GetMonitorInfoW(locate.result, &info))
                return;
            m_indicator = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, L"STATIC", text.c_str(), WS_POPUP | SS_CENTER | SS_CENTERIMAGE, info.rcWork.right - 360, info.rcWork.top + 24, 336, 56, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
            if (m_indicator)
            {
                ShowWindow(m_indicator, SW_SHOWNOACTIVATE);
                m_indicatorUntil = GetTickCount64() + 1400;
            }
        }
        void NotifySelection(const std::wstring& monitor)
        {
            wchar_t text[512]{};
            const auto format = Resource(IDS_WORKSPACE_STATUS);
            if (swprintf_s(text, format.c_str(), m_model.Selected(m_desktop, monitor) + 1, WorkspaceCount) >= 0)
                Notify(monitor, text);
        }
        bool Apply(const Transition& transition)
        {
            if (!transition.valid())
                return false;
            const ULONGLONG deadline = GetTickCount64() + 2000;
            const auto remaining = [&]() -> DWORD { const auto now = GetTickCount64(); return now < deadline ? static_cast<DWORD>(deadline - now) : 0; };
            for (const auto& id : transition.hide)
                if (!m_recovery.Hide(id, remaining()))
                {
                    m_failed = true;
                    return false;
                }
            for (const auto& id : transition.show)
                if (!m_recovery.Show(id, remaining()))
                {
                    m_failed = true;
                    return false;
                }
            if (!m_model.Commit(transition))
            {
                m_failed = true;
                return false;
            }
            return true;
        }

    public:
        explicit Manager(DWORD runner) :
            m_runnerPid(runner) {}
        ~Manager()
        {
            if (m_indicator)
                DestroyWindow(m_indicator);
        }
        bool Start()
        {
            if (FAILED(CoCreateInstance(CLSID_VirtualDesktopManager, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&m_desktops))))
                return false;
            m_desktop = CurrentDesktop(m_desktops.Get());
            m_monitors = ConnectedMonitors();
            return !m_desktop.empty() && !m_monitors.empty() && m_recovery.Start(ExecutablePath()) && Refresh();
        }
        bool Refresh()
        {
            if (!m_recovery.Healthy())
                return false;
            if (m_indicator && GetTickCount64() >= m_indicatorUntil)
            {
                DestroyWindow(m_indicator);
                m_indicator = nullptr;
            }
            const auto monitors = ConnectedMonitors();
            const auto desktop = CurrentDesktop(m_desktops.Get());
            if (desktop.empty() || monitors.empty())
                return false;
            if (monitors != m_monitors)
            {
                if (!m_recovery.RestoreAll())
                    return false;
                m_recovery.UnregisterAll();
                m_model.Reset();
                m_monitors = monitors;
            }
            if (desktop != m_desktop)
            {
                if (!m_recovery.RestoreAll())
                    return false;
                m_model.ReleaseVisibility();
                m_desktop = desktop;
            }
            m_recovery.Prune();
            const auto windows = Collect();
            if (m_failed)
                return false;
            m_model.Observe(m_desktop, windows);
            const HWND foreground = GetForegroundWindow();
            if (foreground && IsWindowVisible(foreground))
            {
                const auto id = m_recovery.Find(foreground);
                const auto workspace = id ? m_model.Assignment(*id) : std::nullopt;
                const auto monitor = MonitorName(MonitorFromWindow(foreground, MONITOR_DEFAULTTONEAREST));
                GUID current{};
                if (workspace && SUCCEEDED(m_desktops->GetWindowDesktopId(foreground, &current)) && DesktopKey(current) == m_desktop && *workspace != m_model.Selected(m_desktop, monitor))
                    if (!Apply(m_model.Select(m_desktop, monitor, *workspace)))
                        return false;
            }
            for (const auto& monitor : m_monitors)
            {
                auto plan = m_model.Select(m_desktop, monitor, m_model.Selected(m_desktop, monitor));
                if (plan.rejection == Rejection::CrossMonitorGroup)
                    continue;
                if ((!plan.hide.empty() || !plan.show.empty()) && !Apply(plan))
                    return false;
            }
            return !m_failed;
        }
        bool Execute(Command command)
        {
            POINT pointer{};
            GetCursorPos(&pointer);
            auto monitor = MonitorName(MonitorFromPoint(pointer, MONITOR_DEFAULTTONEAREST));
            if (command == Command::Restore)
            {
                if (!m_recovery.RestoreAll())
                    return false;
                m_recovery.UnregisterAll();
                m_model.Reset();
                Notify(monitor, Resource(IDS_WINDOWS_RESTORED));
                return true;
            }
            if (!Refresh())
                return false;
            Transition plan;
            if (command == Command::MovePrevious || command == Command::MoveNext)
            {
                const HWND foreground = GetForegroundWindow();
                auto id = m_recovery.Find(foreground);
                if (!id)
                {
                    Notify(monitor, Resource(IDS_OPERATION_REFUSED));
                    return true;
                }
                monitor = MonitorName(MonitorFromWindow(foreground, MONITOR_DEFAULTTONEAREST));
                plan = m_model.Move(*id, command == Command::MovePrevious ? -1 : 1);
            }
            else
                plan = m_model.Step(m_desktop, monitor, command == Command::Previous ? -1 : 1);
            if (!plan.valid())
            {
                Notify(monitor, Resource(IDS_OPERATION_REFUSED));
                return true;
            }
            if (!Apply(plan))
                return false;
            NotifySelection(monitor);
            return true;
        }
    };
    int RunManager(DWORD runnerPid)
    {
        UniqueHandle singleton(CreateMutexW(nullptr, TRUE, ManagerMutex));
        if (!singleton || GetLastError() == ERROR_ALREADY_EXISTS)
            return 3;
        if (!RestoreTaggedWindows())
            return 13;
        std::array<UniqueHandle, static_cast<std::size_t>(Command::Count)> events;
        std::vector<HANDLE> waits;
        for (std::size_t i = 0; i < events.size(); ++i)
        {
            events[i].reset(CreateEventW(nullptr, i == static_cast<std::size_t>(Command::Stop), FALSE, EventNames[i]));
            if (!events[i])
                return 4;
            waits.push_back(events[i].get());
        }
        ShutdownWindow shutdown(events[static_cast<std::size_t>(Command::Stop)].get());
        if (!shutdown)
            return 14;
        if (runnerPid == 0)
            ResetEvent(events[static_cast<std::size_t>(Command::Stop)].get());
        UniqueHandle runner(runnerPid ? OpenProcess(SYNCHRONIZE, FALSE, runnerPid) : nullptr);
        if (runnerPid && !runner)
            return 5;
        if (runner)
            waits.push_back(runner.get());
        UniqueHandle ready(CreateEventW(nullptr, TRUE, FALSE, ReadyEvent));
        if (!ready || FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)))
            return 6;
        int result{};
        try
        {
            Manager manager(runnerPid);
            if (!manager.Start())
                result = 7;
            else
            {
                if (!SetEvent(ready.get()))
                    result = 8;
                while (result == 0)
                {
                    const DWORD wait = MsgWaitForMultipleObjects(static_cast<DWORD>(waits.size()), waits.data(), FALSE, 500, QS_ALLINPUT);
                    if (wait < WAIT_OBJECT_0 + events.size())
                    {
                        const auto command = static_cast<Command>(wait - WAIT_OBJECT_0);
                        if (command == Command::Stop)
                            break;
                        if (!manager.Execute(command))
                            result = 9;
                    }
                    else if (runner && wait == WAIT_OBJECT_0 + events.size())
                        break;
                    else if (wait == WAIT_TIMEOUT)
                    {
                        if (!manager.Refresh())
                            result = 10;
                    }
                    else if (wait == WAIT_OBJECT_0 + waits.size())
                    {
                        MSG message{};
                        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
                        {
                            TranslateMessage(&message);
                            DispatchMessageW(&message);
                        }
                    }
                    else
                        result = 11;
                }
            }
        }
        catch (...)
        {
            result = 12;
        }
        CoUninitialize();
        if (result)
            OutputDebugStringW(L"Independent Desktops stopped; visibility ownership transferred to recovery.\n");
        return result;
    }
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    int argc{};
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv)
        return 2;
    int result = 2;
    if (argc == 5 && std::wstring(argv[1]) == L"--recover")
    {
        HANDLE mapping{}, ready{}, parent{};
        if (ParseHandle(argv[2], mapping) && ParseHandle(argv[3], ready) && ParseHandle(argv[4], parent))
            result = RunRecovery(mapping, ready, parent);
    }
    else if (argc == 1)
        result = RunManager(0);
    else if (argc == 3 && std::wstring(argv[1]) == L"--runner-pid")
    {
        HANDLE number{};
        if (ParseHandle(argv[2], number) && reinterpret_cast<ULONG_PTR>(number) <= MAXDWORD)
            result = RunManager(static_cast<DWORD>(reinterpret_cast<ULONG_PTR>(number)));
    }
    LocalFree(argv);
    return result;
}
