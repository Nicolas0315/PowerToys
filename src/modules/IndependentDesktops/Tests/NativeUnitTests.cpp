// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.
#include "CppUnitTest.h"
#include "CoreCases.h"
#include <cstring>
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
