// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.
#include "Recovery.h"
#include <cerrno>
#include <cstdlib>
#include <limits>
#include <vector>

namespace IndependentDesktops
{
    namespace
    {
        constexpr LONG Free = 0, Registered = 1, Armed = 2;
        constexpr DWORD JournalVersion = 1;
        // Explicit barriers are required for the journal across processes, including
        // ARM64 where plain /volatile:iso reads do not acquire published metadata.
        LONG ReadShared(volatile LONG& value)
        {
            return InterlockedCompareExchange(&value, 0, 0);
        }
        struct Slot
        {
            volatile LONG status{};
            WindowId id{};
            int showCommand{ SW_SHOWNA };
        };
        struct Journal
        {
            DWORD version{ JournalVersion };
            volatile LONG shutdown{};
            Slot slots[MaxTrackedWindows]{};
        };
        struct MappingView
        {
            Journal* data{};
            ~MappingView()
            {
                if (data)
                    UnmapViewOfFile(data);
            }
        };
        struct Attributes
        {
            std::vector<unsigned char> buffer;
            LPPROC_THREAD_ATTRIBUTE_LIST list{};
            ~Attributes()
            {
                if (list)
                    DeleteProcThreadAttributeList(list);
            }
            bool Initialize()
            {
                SIZE_T bytes{};
                InitializeProcThreadAttributeList(nullptr, 1, 0, &bytes);
                if (!bytes)
                    return false;
                buffer.resize(bytes);
                auto candidate = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(buffer.data());
                if (!InitializeProcThreadAttributeList(candidate, 1, 0, &bytes))
                    return false;
                list = candidate;
                return true;
            }
        };
        HWND WindowHandle(const WindowId& id)
        {
            return reinterpret_cast<HWND>(static_cast<ULONG_PTR>(id.handle));
        }
        Slot* FindSlot(Journal* journal, const WindowId& id)
        {
            if (!journal)
                return nullptr;
            for (auto& slot : journal->slots)
                if (ReadShared(slot.status) != Free && slot.id == id)
                    return &slot;
            return nullptr;
        }
        bool RestoreSlot(Slot& slot, DWORD timeout)
        {
            if (ReadShared(slot.status) == Free)
                return true;
            if (!MatchesWindow(slot.id))
            {
                InterlockedExchange(&slot.status, Free);
                return true;
            }
            if (ReadShared(slot.status) == Registered)
                return true;
            if (!ShowWindowAsync(WindowHandle(slot.id), slot.showCommand))
                return false;
            if (!WaitForVisibility(slot.id, true, timeout))
                return false;
            RemovePropW(WindowHandle(slot.id), RestoreIntentProperty);
            InterlockedExchange(&slot.status, Registered);
            return true;
        }
    }
    struct RecoverySession::State
    {
        UniqueHandle mapping;
        MappingView view;
        UniqueHandle watchdog;
        std::uint64_t nextTag{ (GetTickCount64() << 16) ^ GetCurrentProcessId() };
    };
    RecoverySession::RecoverySession() :
        m_state(std::make_unique<State>()) {}
    RecoverySession::~RecoverySession()
    {
        RestoreAll();
        UnregisterAll();
        if (m_state->view.data)
            InterlockedExchange(&m_state->view.data->shutdown, 1);
        if (m_state->watchdog)
            WaitForSingleObject(m_state->watchdog.get(), 1000);
    }
    bool RecoverySession::Start(const std::wstring& executable)
    {
        if (Healthy())
            return true;
        if (m_state->view.data)
            return false; // A failed session must never be silently reused.
        SECURITY_ATTRIBUTES security{ sizeof(security), nullptr, TRUE };
        m_state->mapping.reset(CreateFileMappingW(INVALID_HANDLE_VALUE, &security, PAGE_READWRITE, 0, sizeof(Journal), nullptr));
        UniqueHandle ready(CreateEventW(&security, TRUE, FALSE, nullptr));
        HANDLE parentHandle{};
        if (!m_state->mapping || !ready || !DuplicateHandle(GetCurrentProcess(), GetCurrentProcess(), GetCurrentProcess(), &parentHandle, SYNCHRONIZE, TRUE, 0))
            return false;
        UniqueHandle parent(parentHandle);
        m_state->view.data = static_cast<Journal*>(MapViewOfFile(m_state->mapping.get(), FILE_MAP_ALL_ACCESS, 0, 0, sizeof(Journal)));
        if (!m_state->view.data)
            return false;
        new (m_state->view.data) Journal{};
        Attributes attributes;
        if (!attributes.Initialize())
            return false;
        HANDLE inherited[] = { m_state->mapping.get(), ready.get(), parent.get() };
        if (!UpdateProcThreadAttribute(attributes.list, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inherited, sizeof(inherited), nullptr, nullptr))
            return false;
        std::wstring command = L"\"" + executable + L"\" --recover " +
                               std::to_wstring(reinterpret_cast<ULONG_PTR>(inherited[0])) + L" " +
                               std::to_wstring(reinterpret_cast<ULONG_PTR>(inherited[1])) + L" " +
                               std::to_wstring(reinterpret_cast<ULONG_PTR>(inherited[2]));
        STARTUPINFOEXW startup{};
        startup.StartupInfo.cb = sizeof(startup);
        startup.lpAttributeList = attributes.list;
        PROCESS_INFORMATION process{};
        if (!CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, TRUE, EXTENDED_STARTUPINFO_PRESENT | CREATE_NO_WINDOW, nullptr, nullptr, &startup.StartupInfo, &process))
            return false;
        m_state->watchdog.reset(process.hProcess);
        UniqueHandle thread(process.hThread);
        HANDLE waits[] = { ready.get(), m_state->watchdog.get() };
        if (WaitForMultipleObjects(2, waits, FALSE, 5000) != WAIT_OBJECT_0)
            return false;
        return Healthy();
    }
    bool RecoverySession::Healthy() const
    {
        return m_state->view.data && m_state->watchdog && WaitForSingleObject(m_state->watchdog.get(), 0) == WAIT_TIMEOUT;
    }
    HANDLE RecoverySession::ProcessHandle() const
    {
        return m_state->watchdog.get();
    }
    std::optional<WindowId> RecoverySession::Find(HWND window) const
    {
        if (!m_state->view.data)
            return {};
        for (auto& slot : m_state->view.data->slots)
            if (ReadShared(slot.status) != Free && WindowHandle(slot.id) == window && MatchesWindow(slot.id))
                return slot.id;
        return {};
    }
    std::size_t RecoverySession::RegisteredCount() const
    {
        std::size_t count{};
        if (m_state->view.data)
            for (auto& slot : m_state->view.data->slots)
                if (ReadShared(slot.status) != Free)
                    ++count;
        return count;
    }
    std::optional<WindowId> RecoverySession::Register(HWND window)
    {
        if (!Healthy())
            return {};
        if (auto existing = Find(window))
            return existing;
        auto id = IdentifyWindow(window);
        if (!id || !IsWindowVisible(window) || id->incarnation != 0)
            return {};
        Prune();
        for (auto& slot : m_state->view.data->slots)
        {
            if (ReadShared(slot.status) != Free)
                continue;
            if (++m_state->nextTag == 0)
                ++m_state->nextTag;
            id->incarnation = m_state->nextTag;
            slot.id = *id;
            slot.showCommand = IsIconic(window) ? SW_SHOWMINNOACTIVE : SW_SHOWNA;
            // Register before publishing the property: death at either instruction is safe.
            InterlockedExchange(&slot.status, Registered);
            if (!SetPropW(window, RecoveryProperty, reinterpret_cast<HANDLE>(static_cast<ULONG_PTR>(id->incarnation))) || !MatchesWindow(*id))
            {
                if (MatchesWindow(*id))
                    RemovePropW(window, RecoveryProperty);
                InterlockedExchange(&slot.status, Free);
                return {};
            }
            return id;
        }
        return {};
    }
    void RecoverySession::Prune()
    {
        if (!m_state->view.data)
            return;
        for (auto& slot : m_state->view.data->slots)
            if (ReadShared(slot.status) != Free && !MatchesWindow(slot.id))
                InterlockedExchange(&slot.status, Free);
    }
    bool RecoverySession::Hide(const WindowId& id, DWORD timeoutMs)
    {
        if (!Healthy() || !MatchesWindow(id))
            return false;
        auto* slot = FindSlot(m_state->view.data, id);
        if (!slot)
            return false;
        const HWND window = WindowHandle(id);
        if (ReadShared(slot->status) == Registered && !IsWindowVisible(window))
            return false;
        if (ReadShared(slot->status) == Registered)
            slot->showCommand = IsIconic(window) ? SW_SHOWMINNOACTIVE : SW_SHOWNA;
        // Publish recovery intent before issuing any asynchronous visibility operation.
        InterlockedExchange(&slot->status, Armed);
        if (!SetPropW(window, RestoreIntentProperty, reinterpret_cast<HANDLE>(static_cast<ULONG_PTR>(slot->showCommand))))
            return false;
        return ShowWindowAsync(window, SW_HIDE) && WaitForVisibility(id, false, timeoutMs);
    }
    bool RecoverySession::Show(const WindowId& id, DWORD timeoutMs)
    {
        auto* slot = FindSlot(m_state->view.data, id);
        return !slot || RestoreSlot(*slot, timeoutMs);
    }
    bool RecoverySession::RestoreAll()
    {
        if (!m_state->view.data)
            return true;
        const ULONGLONG deadline = GetTickCount64() + 2000;
        bool restored = true;
        // Queue every restore first, including windows whose application is unresponsive.
        for (auto& slot : m_state->view.data->slots)
            if (ReadShared(slot.status) == Armed && MatchesWindow(slot.id))
                ShowWindowAsync(WindowHandle(slot.id), slot.showCommand);
        for (auto& slot : m_state->view.data->slots)
        {
            const auto now = GetTickCount64();
            const DWORD remaining = now < deadline ? static_cast<DWORD>(deadline - now) : 0;
            if (!RestoreSlot(slot, remaining))
                restored = false;
        }
        return restored;
    }
    void RecoverySession::UnregisterAll()
    {
        if (!m_state->view.data)
            return;
        for (auto& slot : m_state->view.data->slots)
        {
            // Never discard the recovery intent of an unacknowledged restore.
            if (ReadShared(slot.status) == Armed)
                continue;
            if (ReadShared(slot.status) == Registered && MatchesWindow(slot.id))
            {
                RemovePropW(WindowHandle(slot.id), RestoreIntentProperty);
                RemovePropW(WindowHandle(slot.id), RecoveryProperty);
            }
            InterlockedExchange(&slot.status, Free);
        }
    }
    int RunRecovery(HANDLE mappingHandle, HANDLE readyHandle, HANDLE parentHandle)
    {
        UniqueHandle mapping(mappingHandle), ready(readyHandle), parent(parentHandle);
        MappingView view;
        view.data = static_cast<Journal*>(MapViewOfFile(mapping.get(), FILE_MAP_ALL_ACCESS, 0, 0, sizeof(Journal)));
        if (!view.data || view.data->version != JournalVersion || !SetEvent(ready.get()))
            return 2;
        while (ReadShared(view.data->shutdown) == 0)
        {
            const DWORD wait = WaitForSingleObject(parent.get(), 100);
            if (wait == WAIT_OBJECT_0)
                break;
            if (wait != WAIT_TIMEOUT)
                return 3;
        }
        // The manager has exited or transferred ownership; it no longer writes the mapping.
        // Retry unresponsive applications instead of declaring their windows recovered.
        bool remaining{};
        do
        {
            remaining = false;
            for (auto& slot : view.data->slots)
            {
                if (!RestoreSlot(slot, 0))
                {
                    remaining = true;
                    continue;
                }
                if (ReadShared(slot.status) == Registered && MatchesWindow(slot.id))
                {
                    RemovePropW(WindowHandle(slot.id), RestoreIntentProperty);
                    RemovePropW(WindowHandle(slot.id), RecoveryProperty);
                }
                InterlockedExchange(&slot.status, Free);
            }
            if (remaining)
                Sleep(100);
        } while (remaining);
        return 0;
    }
    bool ParseHandle(const wchar_t* text, HANDLE& handle)
    {
        if (!text || !*text || *text < L'0' || *text > L'9')
            return false;
        errno = 0;
        wchar_t* end{};
        const auto value = std::wcstoull(text, &end, 10);
        if (errno == ERANGE || !end || *end != L'\0' || value == 0 || value >= std::numeric_limits<ULONG_PTR>::max())
            return false;
        handle = reinterpret_cast<HANDLE>(static_cast<ULONG_PTR>(value));
        return true;
    }
}
