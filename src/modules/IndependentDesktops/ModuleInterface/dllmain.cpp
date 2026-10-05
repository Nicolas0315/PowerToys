// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.
#include "pch.h"

#include <common/SettingsAPI/settings_objects.h>
#include <common/utils/gpo.h>
#include <interface/powertoy_module_interface.h>

#include "../Constants.h"
#include "resource.h"

extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace
{
    constexpr wchar_t PropertiesKey[] = L"properties";
    constexpr wchar_t WinKey[] = L"win";
    constexpr wchar_t AltKey[] = L"alt";
    constexpr wchar_t CtrlKey[] = L"ctrl";
    constexpr wchar_t ShiftKey[] = L"shift";
    constexpr wchar_t CodeKey[] = L"code";

    constexpr size_t HotkeyCount = static_cast<size_t>(IndependentDesktops::Command::Stop);
    constexpr DWORD ChildExitTimeoutMs = 2000;
    constexpr DWORD ChildReadyTimeoutMs = 5000;

    PowertoyModuleIface::Hotkey DefaultHotkey(IndependentDesktops::Command command)
    {
        const bool isPrevious = command == IndependentDesktops::Command::Previous || command == IndependentDesktops::Command::MovePrevious;
        const bool isMove = command == IndependentDesktops::Command::MovePrevious || command == IndependentDesktops::Command::MoveNext;
        PowertoyModuleIface::Hotkey hotkey{};
        hotkey.win = true;
        hotkey.ctrl = true;
        hotkey.alt = true;
        hotkey.shift = isMove;
        hotkey.key = command == IndependentDesktops::Command::Restore ? VK_HOME : (isPrevious ? VK_LEFT : VK_RIGHT);
        return hotkey;
    }

    bool ParseHotkey(const winrt::Windows::Data::Json::JsonObject& properties, const wchar_t* key, PowertoyModuleIface::Hotkey& hotkey)
    {
        try
        {
            const auto value = properties.GetNamedObject(key);
            PowertoyModuleIface::Hotkey parsed{};
            parsed.win = value.GetNamedBoolean(WinKey);
            parsed.alt = value.GetNamedBoolean(AltKey);
            parsed.ctrl = value.GetNamedBoolean(CtrlKey);
            parsed.shift = value.GetNamedBoolean(ShiftKey);
            const double code = value.GetNamedNumber(CodeKey);
            if (!std::isfinite(code) || std::floor(code) != code || code < 1 || code > 0xFF || !(parsed.win || parsed.alt || parsed.ctrl || parsed.shift))
            {
                Logger::warn("Ignoring invalid Independent Desktops shortcut");
                return false;
            }
            parsed.key = static_cast<unsigned char>(code);
            hotkey = parsed;
            return true;
        }
        catch (...)
        {
            Logger::error("Failed to load Independent Desktops shortcut from settings");
            return false;
        }
    }
}

class IndependentDesktopsModuleInterface final : public PowertoyModuleIface
{
public:
    IndependentDesktopsModuleInterface()
    {
        LoggerHelpers::init_logger(IndependentDesktops::ModuleKey, L"ModuleInterface", "IndependentDesktops");
        for (size_t i = 0; i < m_events.size(); ++i)
        {
            const bool manualReset = i == static_cast<size_t>(IndependentDesktops::Command::Stop);
            m_events[i] = CreateEventW(nullptr, manualReset, FALSE, IndependentDesktops::EventNames[i]);
            if (!m_events[i])
            {
                Logger::error(L"Could not create Independent Desktops event {}", IndependentDesktops::EventNames[i]);
            }
        }
        m_readyEvent = CreateEventW(nullptr, TRUE, FALSE, IndependentDesktops::ReadyEvent);
        if (!m_readyEvent)
        {
            Logger::error(L"Could not create Independent Desktops ready event {}", IndependentDesktops::ReadyEvent);
        }
        init_settings();
    }

    ~IndependentDesktopsModuleInterface() override
    {
        disable();
        for (HANDLE event : m_events)
        {
            if (event)
            {
                CloseHandle(event);
            }
        }
        if (m_readyEvent)
        {
            CloseHandle(m_readyEvent);
        }
    }

    void destroy() override
    {
        delete this;
    }

    const wchar_t* get_name() override { return m_name.c_str(); }
    const wchar_t* get_key() override { return IndependentDesktops::ModuleKey; }

    powertoys_gpo::gpo_rule_configured_t gpo_policy_enabled_configuration() override
    {
        return powertoys_gpo::getConfiguredIndependentDesktopsEnabledValue();
    }

    bool get_config(wchar_t* buffer, int* buffer_size) override
    {
        PowerToysSettings::Settings settings(reinterpret_cast<HINSTANCE>(&__ImageBase), get_name());
        settings.set_description(m_description);
        return settings.serialize_to_buffer(buffer, buffer_size);
    }

    void call_custom_action(const wchar_t*) override {}

    void set_config(const wchar_t* config) override
    {
        try
        {
            auto values = PowerToysSettings::PowerToyValues::from_json_string(config, get_key());
            parse_settings(values);
            values.save_to_settings_file();
        }
        catch (const std::exception&)
        {
            Logger::error("Invalid Independent Desktops settings");
        }
    }

    void enable() override
    {
        if (m_enabled)
        {
            return;
        }
        if (!events_ready())
        {
            m_enabled = false;
            return;
        }

        stop_runner();
        ResetEvent(m_events[static_cast<size_t>(IndependentDesktops::Command::Stop)]);
        ResetEvent(m_readyEvent);
        m_enabled = start_runner();
    }

    void disable() override
    {
        m_enabled = false;
        const HANDLE stopEvent = m_events[static_cast<size_t>(IndependentDesktops::Command::Stop)];
        if (stopEvent)
        {
            SetEvent(stopEvent);
        }
        stop_runner();
    }

    bool is_enabled() override
    {
        if (m_enabled && m_runnerProcess && WaitForSingleObject(m_runnerProcess, 0) == WAIT_OBJECT_0)
        {
            Logger::warn("Independent Desktops runner exited unexpectedly");
            CloseHandle(m_runnerProcess);
            m_runnerProcess = nullptr;
            m_enabled = false;
        }
        return m_enabled;
    }
    bool is_enabled_by_default() const override { return false; }

    size_t get_hotkeys(Hotkey* hotkeys, size_t buffer_size) override
    {
        if (hotkeys && buffer_size >= HotkeyCount)
        {
            for (size_t i = 0; i < HotkeyCount; ++i)
            {
                hotkeys[i] = m_hotkeys[i];
            }
        }
        return HotkeyCount;
    }

    bool on_hotkey(size_t id) override
    {
        if (!m_enabled || id >= HotkeyCount || !m_events[id])
        {
            return false;
        }
        return SetEvent(m_events[id]) != FALSE;
    }

private:
    void init_settings()
    {
        reset_hotkeys_to_defaults();
        try
        {
            auto values = PowerToysSettings::PowerToyValues::load_from_settings_file(get_key());
            parse_settings(values);
        }
        catch (const std::exception&)
        {
            Logger::info("Using default Independent Desktops shortcuts");
        }
    }

    void reset_hotkeys_to_defaults()
    {
        for (size_t i = 0; i < HotkeyCount; ++i)
        {
            m_hotkeys[i] = DefaultHotkey(static_cast<IndependentDesktops::Command>(i));
        }
    }

    void parse_settings(PowerToysSettings::PowerToyValues& settings)
    {
        reset_hotkeys_to_defaults();
        const auto root = settings.get_raw_json();
        if (!root.HasKey(PropertiesKey))
        {
            return;
        }
        const auto properties = root.GetNamedObject(PropertiesKey);
        for (size_t i = 0; i < HotkeyCount; ++i)
        {
            if (properties.HasKey(IndependentDesktops::ShortcutKeys[i]))
            {
                ParseHotkey(properties, IndependentDesktops::ShortcutKeys[i], m_hotkeys[i]);
            }
        }
    }

    bool events_ready() const
    {
        for (HANDLE event : m_events)
        {
            if (!event)
            {
                return false;
            }
        }
        return m_readyEvent != nullptr;
    }

    bool start_runner()
    {
        wchar_t path[MAX_PATH]{};
        if (!GetModuleFileNameW(reinterpret_cast<HMODULE>(&__ImageBase), path, ARRAYSIZE(path)))
        {
            return false;
        }
        std::wstring executable(path);
        executable.resize(executable.find_last_of(L"\\/") + 1);
        executable += IndependentDesktops::ExecutableName;

        std::wstring commandLine = L"\"" + executable + L"\" --runner-pid " + std::to_wstring(GetCurrentProcessId());
        STARTUPINFOW startupInfo{ .cb = sizeof(startupInfo) };
        PROCESS_INFORMATION processInfo{};
        if (!CreateProcessW(executable.c_str(), commandLine.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startupInfo, &processInfo))
        {
            Logger::error(L"Could not start {}", executable);
            return false;
        }
        CloseHandle(processInfo.hThread);
        m_runnerProcess = processInfo.hProcess;
        const HANDLE handles[] = { m_readyEvent, m_runnerProcess };
        // Module enablement has no completion callback. Keep this bounded so the runner
        // never registers hotkeys for a process that has not initialized its recovery state.
        const DWORD result = WaitForMultipleObjects(ARRAYSIZE(handles), handles, FALSE, ChildReadyTimeoutMs);
        if (result == WAIT_OBJECT_0)
        {
            return true;
        }
        if (result == WAIT_OBJECT_0 + 1)
        {
            Logger::error("Independent Desktops runner exited before it became ready");
        }
        else if (result == WAIT_TIMEOUT)
        {
            Logger::error("Independent Desktops runner did not become ready within {} ms", ChildReadyTimeoutMs);
        }
        else
        {
            Logger::error("Independent Desktops ready wait failed: {}", result);
        }
        SetEvent(m_events[static_cast<size_t>(IndependentDesktops::Command::Stop)]);
        stop_runner();
        return false;
    }

    void stop_runner()
    {
        if (!m_runnerProcess)
        {
            return;
        }
        SetEvent(m_events[static_cast<size_t>(IndependentDesktops::Command::Stop)]);
        if (WaitForSingleObject(m_runnerProcess, ChildExitTimeoutMs) == WAIT_TIMEOUT)
        {
            Logger::warn("Independent Desktops runner did not stop in time; terminating it");
            TerminateProcess(m_runnerProcess, 1);
            WaitForSingleObject(m_runnerProcess, ChildExitTimeoutMs);
        }
        CloseHandle(m_runnerProcess);
        m_runnerProcess = nullptr;
    }

    bool m_enabled = false;
    HANDLE m_runnerProcess = nullptr;
    HANDLE m_readyEvent = nullptr;
    std::wstring m_name = GET_RESOURCE_STRING(IDS_INDEPENDENT_DESKTOPS_NAME);
    std::wstring m_description = GET_RESOURCE_STRING(IDS_INDEPENDENT_DESKTOPS_SETTINGS_DESC);
    std::array<HANDLE, static_cast<size_t>(IndependentDesktops::Command::Count)> m_events{};
    std::array<Hotkey, HotkeyCount> m_hotkeys{};
};

extern "C" __declspec(dllexport) PowertoyModuleIface* __cdecl powertoy_create()
{
    return new IndependentDesktopsModuleInterface();
}
