// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.
#include "CppUnitTest.h"
#include "CoreCases.h"
#include <cstring>
#include <filesystem>
#include <memory>
#include <windows.h>
#include <roapi.h>
#include <modules/interface/powertoy_module_interface.h>
using namespace Microsoft::VisualStudio::CppUnitTestFramework;
namespace IndependentDesktopsTests
{
    TEST_CLASS (WorkspaceContracts)
    {
        static void Run(const std::string& wanted)
        {
            for (const auto& [name, body] : CoreCases())
            {
                if (name != wanted)
                    continue;
                try
                {
                    body();
                }
                catch (const std::exception& error)
                {
                    Assert::Fail(std::wstring(error.what(), error.what() + std::strlen(error.what())).c_str());
                }
                return;
            }
            Assert::Fail(L"Unknown test case");
        }

    public:
        TEST_METHOD (malformed_settings_do_not_escape_the_module_boundary)
        {
            struct Apartment
            {
                HRESULT result = RoInitialize(RO_INIT_MULTITHREADED);
                ~Apartment()
                {
                    if (SUCCEEDED(result))
                        RoUninitialize();
                }
            } apartment;
            Assert::IsTrue(SUCCEEDED(apartment.result) || apartment.result == RPC_E_CHANGED_MODE, L"WinRT apartment initialized");
            const auto testModule = GetModuleHandleW(L"IndependentDesktopsUnitTests.dll");
            wchar_t location[32768]{};
            Assert::IsTrue(testModule != nullptr && GetModuleFileNameW(testModule, location, 32768) != 0, L"Locate built test module");
            const auto executableDirectory = std::filesystem::path(location).parent_path().parent_path().parent_path();
            struct Library
            {
                HMODULE module{};
                ~Library()
                {
                    if (module)
                        FreeLibrary(module);
                }
            } library{ LoadLibraryExW((executableDirectory / L"PowerToys.IndependentDesktopsModuleInterface.dll").c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS) };
            Assert::IsTrue(library.module != nullptr, L"Load the repository-built module DLL");
            using Factory = PowertoyModuleIface*(__cdecl*)();
            const auto factory = reinterpret_cast<Factory>(GetProcAddress(library.module, "powertoy_create"));
            Assert::IsTrue(factory != nullptr, L"Resolve module factory");
            const auto destroy = [](PowertoyModuleIface* module) { if (module) module->destroy(); };
            std::unique_ptr<PowertoyModuleIface, decltype(destroy)> module(factory(), destroy);
            Assert::IsTrue(module != nullptr && !module->is_enabled(), L"Synthetic settings test never enables desktop management");
            module->set_config(LR"({"name":"IndependentDesktops","properties":{"previous_desktop":{"win":true,"ctrl":true,"alt":true,"shift":false,"code":65}}})");
            const wchar_t* inputs[] = {
                L"{",
                L"null",
                L"[]",
                L"{}",
                LR"({"name":"IndependentDesktops","properties":null})",
                LR"({"name":"IndependentDesktops","properties":[]})",
                LR"({"name":"IndependentDesktops","properties":{"previous_desktop":false}})",
                LR"({"name":"IndependentDesktops","properties":{"previous_desktop":{"win":true,"ctrl":true,"alt":true,"shift":false,"code":256}}})",
                LR"({"name":"IndependentDesktops","properties":{"previous_desktop":{"win":true,"ctrl":true,"alt":true,"shift":false,"code":37.5}}})",
                LR"({"name":"IndependentDesktops","properties":{"previous_desktop":{"win":false,"ctrl":false,"alt":false,"shift":false,"code":37}}})",
                LR"({"name":"IndependentDesktops","properties":{"previous_desktop":{"win":"true","ctrl":true,"alt":true,"shift":false,"code":37}}})"
            };
            for (const auto* input : inputs)
            {
                try
                {
                    module->set_config(input);
                }
                catch (...)
                {
                    Assert::Fail(L"Invalid settings escaped the module boundary");
                }
                PowertoyModuleIface::Hotkey hotkeys[5]{};
                Assert::AreEqual(std::size_t{ 5 }, module->get_hotkeys(hotkeys, 5));
                Assert::AreEqual(65, static_cast<int>(hotkeys[0].key), L"Rejected input preserves the prior valid shortcut");
                for (const auto& hotkey : hotkeys)
                    Assert::IsTrue(hotkey.key != 0 && (hotkey.win || hotkey.ctrl || hotkey.alt || hotkey.shift), L"Rejected input preserves valid hotkeys");
                Assert::IsFalse(module->is_enabled(), L"Settings parsing cannot start desktop management");
            }
        }
        TEST_METHOD (foreground_cross_monitor_dialog_cannot_select_unrelated_monitor)
        {
            Run("foreground_cross_monitor_dialog_cannot_select_unrelated_monitor");
        }
        TEST_METHOD (app_hidden_cross_monitor_dialog_still_blocks_group_switch)
        {
            Run("app_hidden_cross_monitor_dialog_still_blocks_group_switch");
        }
        TEST_METHOD (window_marker_generation_prevents_same_process_handle_reuse)
        {
            Run("window_marker_generation_prevents_same_process_handle_reuse");
        }
        TEST_METHOD (fixed_seed_10000_transitions_preserve_independent_monitors)
        {
            Run("fixed_seed_10000_transitions_preserve_independent_monitors");
        }
        TEST_METHOD (switch_only_target_monitor)
        {
            Run("switch_only_target_monitor");
        }
        TEST_METHOD (switch_back_restores_only_utility_hidden_windows)
        {
            Run("switch_back_restores_only_utility_hidden_windows");
        }
        TEST_METHOD (hidden_windows_are_not_adopted)
        {
            Run("hidden_windows_are_not_adopted");
        }
        TEST_METHOD (application_hidden_window_is_not_shown)
        {
            Run("application_hidden_window_is_not_shown");
        }
        TEST_METHOD (new_window_joins_selected_workspace)
        {
            Run("new_window_joins_selected_workspace");
        }
        TEST_METHOD (move_group_leaves_both_monitor_selections_unchanged)
        {
            Run("move_group_leaves_both_monitor_selections_unchanged");
        }
        TEST_METHOD (cross_monitor_owned_group_blocks_switch)
        {
            Run("cross_monitor_owned_group_blocks_switch");
        }
        TEST_METHOD (uncommitted_transition_does_not_change_selection)
        {
            Run("uncommitted_transition_does_not_change_selection");
        }
        TEST_METHOD (stale_plan_cannot_commit)
        {
            Run("stale_plan_cannot_commit");
        }
        TEST_METHOD (native_desktop_selections_are_separate)
        {
            Run("native_desktop_selections_are_separate");
        }
        TEST_METHOD (reused_handle_has_new_membership)
        {
            Run("reused_handle_has_new_membership");
        }
        TEST_METHOD (visible_window_moved_to_another_monitor_joins_its_current_workspace)
        {
            Run("visible_window_moved_to_another_monitor_joins_its_current_workspace");
        }
        TEST_METHOD (workspace_wrap_and_invalid_selection)
        {
            Run("workspace_wrap_and_invalid_selection");
        }
        TEST_METHOD (restore_resets_state)
        {
            Run("restore_resets_state");
        }
        TEST_METHOD (capacity_exhaustion_refuses_visibility_changes)
        {
            Run("capacity_exhaustion_refuses_visibility_changes");
        }
    };
}
