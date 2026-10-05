// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.
#include "../Recovery.h"
#include <functional>
#include <iostream>
#include <stdexcept>

using namespace IndependentDesktops;
static void Check(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}
static std::wstring Executable()
{
    wchar_t path[32768]{};
    const DWORD length = GetModuleFileNameW(nullptr, path, 32768);
    Check(length != 0 && length < 32768, "get executable path");
    return path;
}
struct Fixture
{
    HWND window = CreateWindowExW(0, L"STATIC", L"Synthetic independent desktop test", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 50, 50, 200, 100, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    Fixture() { Check(window != nullptr, "create synthetic window"); }
    ~Fixture()
    {
        if (window)
            DestroyWindow(window);
    }
};
static bool PumpVisibility(HWND window, bool visible, DWORD timeout = 5000)
{
    const auto deadline = GetTickCount64() + timeout;
    do
    {
        MSG message{};
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        if ((IsWindowVisible(window) != FALSE) == visible)
            return true;
        Sleep(10);
    } while (GetTickCount64() < deadline);
    return false;
}
int wmain(int argc, wchar_t** argv)
{
    if (argc == 5 && std::wstring(argv[1]) == L"--recover")
    {
        HANDLE mapping{}, ready{}, parent{};
        if (!ParseHandle(argv[2], mapping) || !ParseHandle(argv[3], ready) || !ParseHandle(argv[4], parent))
            return 2;
        return RunRecovery(mapping, ready, parent);
    }
    if (argc == 3 && (std::wstring(argv[1]) == L"--crash-owner" || std::wstring(argv[1]) == L"--crash-both-owner"))
    {
        HANDLE value{};
        if (!ParseHandle(argv[2], value))
            return 2;
        RecoverySession session;
        if (!session.Start(Executable()))
            return 3;
        const auto id = session.Register(static_cast<HWND>(value));
        if (!id || !session.Hide(*id))
            return 4;
        if (std::wstring(argv[1]) == L"--crash-both-owner")
        {
            if (!TerminateProcess(session.ProcessHandle(), 99) || WaitForSingleObject(session.ProcessHandle(), 5000) != WAIT_OBJECT_0)
                return 6;
        }
        // Deliberately bypass destructors; the watchdog must restore the foreign window.
        TerminateProcess(GetCurrentProcess(), 88);
        return 5;
    }
    unsigned failures = 0;
    const auto test = [&](const char* name, const std::function<void()>& run) {
        try
        {
            run();
            std::cout << "PASS " << name << '\n';
        }
        catch (const std::exception& error)
        {
            ++failures;
            std::cout << "FAIL " << name << ": " << error.what() << '\n';
        }
    };
    test("installer_close_requests_stop_without_touching_application", [] {
        Fixture fixture;
        UniqueHandle stop(CreateEventW(nullptr, TRUE, FALSE, nullptr));
        ShutdownWindow shutdown(stop.get());
        Check(static_cast<bool>(shutdown), "create shutdown endpoint");
        Check(!IsWindowVisible(shutdown.get()), "shutdown endpoint stays hidden");
        Check(WaitForSingleObject(stop.get(), 0) == WAIT_TIMEOUT, "no initial shutdown request");
        SendMessageW(shutdown.get(), WM_CLOSE, 0, 0);
        Check(WaitForSingleObject(stop.get(), 0) == WAIT_OBJECT_0, "installer close signals stop");
        Check(IsWindowVisible(fixture.window), "application window stays visible");
    });
    test("identity_of_real_window", [] {
        Fixture fixture;
        auto id = IdentifyWindow(fixture.window);
        Check(id.has_value() && id->process == GetCurrentProcessId(), "real identity discovered");
        Check(MatchesWindow(*id, false), "unmarked identity matches");
        id->created += 1;
        Check(!MatchesWindow(*id, false), "creation time mismatch rejected");
    });
    test("acknowledged_watchdog_required_before_hide", [] {
        Fixture fixture;
        RecoverySession session;
        Check(!session.Register(fixture.window), "no registration without a live watchdog");
        Check(IsWindowVisible(fixture.window), "window stays visible");
    });
    test("hide_and_restore_preserve_window", [] {
        Fixture fixture;
        RecoverySession session;
        Check(session.Start(Executable()), "watchdog ready handshake");
        auto id = session.Register(fixture.window);
        Check(id.has_value(), "register initially visible window");
        Check(session.Hide(*id) && !IsWindowVisible(fixture.window), "hide acknowledged");
        Check(session.RestoreAll() && IsWindowVisible(fixture.window), "restore acknowledged");
        Check(IsWindow(fixture.window), "window not closed");
    });
    test("already_hidden_window_is_not_registered", [] {
        Fixture fixture;
        ShowWindow(fixture.window, SW_HIDE);
        RecoverySession session;
        Check(session.Start(Executable()), "watchdog ready");
        Check(!session.Register(fixture.window), "app-hidden windows excluded");
        session.RestoreAll();
        Check(!IsWindowVisible(fixture.window), "originally hidden window stays hidden");
    });
    test("marker_mismatch_cannot_restore_reused_identity", [] {
        Fixture fixture;
        RecoverySession session;
        Check(session.Start(Executable()), "watchdog ready");
        const auto id = session.Register(fixture.window);
        Check(id && session.Hide(*id), "hide");
        Check(SetPropW(fixture.window, RecoveryProperty, reinterpret_cast<HANDLE>(static_cast<ULONG_PTR>(id->incarnation + 1))), "replace incarnation marker");
        Check(session.RestoreAll(), "mismatched window is no longer owned");
        Check(!IsWindowVisible(fixture.window), "mismatched handle never shown");
        RemovePropW(fixture.window, RecoveryProperty);
    });
    test("minimized_state_survives_restore", [] {
        Fixture fixture;
        ShowWindow(fixture.window, SW_SHOWMINNOACTIVE);
        RecoverySession session;
        Check(session.Start(Executable()), "watchdog ready");
        const auto id = session.Register(fixture.window);
        Check(id && session.Hide(*id), "hide minimized window");
        Check(session.RestoreAll() && IsIconic(fixture.window), "minimized state retained");
    });
    test("watchdog_death_refuses_new_hides", [] {
        Fixture fixture;
        RecoverySession session;
        Check(session.Start(Executable()), "watchdog ready");
        auto id = session.Register(fixture.window);
        Check(id.has_value(), "register");
        Check(TerminateProcess(session.ProcessHandle(), 99), "kill synthetic watchdog");
        Check(WaitForSingleObject(session.ProcessHandle(), 5000) == WAIT_OBJECT_0, "watchdog exited");
        Check(!session.Hide(*id) && IsWindowVisible(fixture.window), "no unprotected hiding");
    });
    test("crashed_owner_restores_foreign_process_window", [] {
        Fixture fixture;
        const auto executable = Executable();
        std::wstring command = L"\"" + executable + L"\" --crash-owner " + std::to_wstring(reinterpret_cast<ULONG_PTR>(fixture.window));
        STARTUPINFOW startup{ sizeof(startup) };
        PROCESS_INFORMATION process{};
        Check(CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startup, &process), "start crash owner");
        UniqueHandle child(process.hProcess), thread(process.hThread);
        const auto deadline = GetTickCount64() + 10000;
        while (WaitForSingleObject(child.get(), 0) == WAIT_TIMEOUT && GetTickCount64() < deadline)
        {
            MSG message{};
            while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
            {
                TranslateMessage(&message);
                DispatchMessageW(&message);
            }
            Sleep(10);
        }
        Check(WaitForSingleObject(child.get(), 0) == WAIT_OBJECT_0, "owner exited");
        DWORD exit{};
        GetExitCodeProcess(child.get(), &exit);
        Check(exit == 88, "owner hid window before forced termination");
        Check(PumpVisibility(fixture.window, true), "watchdog restored foreign window");
        const auto cleanupDeadline = GetTickCount64() + 5000;
        while (GetPropW(fixture.window, RecoveryProperty) && GetTickCount64() < cleanupDeadline)
            Sleep(10);
        Check(GetPropW(fixture.window, RecoveryProperty) == nullptr, "watchdog removed ownership marker");
    });
    test("next_start_recovers_windows_after_both_processes_die", [] {
        Fixture fixture;
        const auto executable = Executable();
        std::wstring command = L"\"" + executable + L"\" --crash-both-owner " + std::to_wstring(reinterpret_cast<ULONG_PTR>(fixture.window));
        STARTUPINFOW startup{ sizeof(startup) };
        PROCESS_INFORMATION process{};
        Check(CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startup, &process), "start double-fault owner");
        UniqueHandle child(process.hProcess), thread(process.hThread);
        const auto deadline = GetTickCount64() + 10000;
        while (WaitForSingleObject(child.get(), 0) == WAIT_TIMEOUT && GetTickCount64() < deadline)
        {
            MSG message{};
            while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
            {
                TranslateMessage(&message);
                DispatchMessageW(&message);
            }
            Sleep(10);
        }
        DWORD exit{};
        GetExitCodeProcess(child.get(), &exit);
        Check(exit == 88 && !IsWindowVisible(fixture.window), "both owner and watchdog died after hide");
        UniqueHandle exclusive(CreateMutexW(nullptr, TRUE, ManagerMutex));
        Check(exclusive && GetLastError() != ERROR_ALREADY_EXISTS, "orphan recovery requires an isolated manager");
        Check(RestoreTaggedWindows(), "restore orphaned visibility intent");
        Check(IsWindowVisible(fixture.window) && !GetPropW(fixture.window, RecoveryProperty), "next start restores and clears markers");
    });
    std::cout << "RESULT failures=" << failures << '\n';
    return failures == 0 ? 0 : 1;
}
