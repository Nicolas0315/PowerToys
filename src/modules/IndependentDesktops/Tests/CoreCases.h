#pragma once
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.
#include "../WorkspaceModel.h"
#include <functional>
#include <random>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace IndependentDesktops;
static void Check(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}
static WindowSnapshot Window(std::uint64_t id, const wchar_t* monitor, const wchar_t* desktop = L"native-1", bool visible = true)
{
    WindowId identity{ id, 10, 20, 30 };
    return { identity, identity, monitor, desktop, visible, true };
}
static bool Has(const std::vector<WindowId>& values, const WindowId& id)
{
    for (const auto& value : values)
        if (value == id)
            return true;
    return false;
}
inline std::vector<std::pair<std::string, std::function<void()>>> CoreCases()
{
    std::vector<std::pair<std::string, std::function<void()>>> cases;
    cases.emplace_back("switch_only_target_monitor", [] {
        WorkspaceModel model;
        auto left = Window(1, L"left"), right = Window(2, L"right");
        model.Observe(L"native-1", { left, right });
        auto change = model.Step(L"native-1", L"right", 1);
        Check(change.valid(), "switch must be valid");
        Check(change.hide.size() == 1 && Has(change.hide, right.id), "only right window hides");
        Check(model.Commit(change), "commit succeeds");
        Check(model.Selected(L"native-1", L"left") == 0, "left selection stays unchanged");
        Check(model.Selected(L"native-1", L"right") == 1, "right advances");
    });
    cases.emplace_back("switch_back_restores_only_utility_hidden_windows", [] {
        WorkspaceModel model;
        auto window = Window(1, L"left");
        model.Observe(L"native-1", { window });
        Check(model.Commit(model.Step(L"native-1", L"left", 1)), "first switch");
        window.visible = false;
        model.Observe(L"native-1", { window });
        auto back = model.Step(L"native-1", L"left", -1);
        Check(back.valid() && Has(back.show, window.id), "utility hidden window must return");
    });
    cases.emplace_back("hidden_windows_are_not_adopted", [] {
        WorkspaceModel model;
        model.Observe(L"native-1", { Window(1, L"left", L"native-1", false) });
        Check(model.size() == 0, "app-hidden window must stay unmanaged");
    });
    cases.emplace_back("application_hidden_window_is_not_shown", [] {
        WorkspaceModel model;
        auto window = Window(1, L"left");
        model.Observe(L"native-1", { window });
        window.visible = false;
        model.Observe(L"native-1", { window });
        Check(model.Select(L"native-1", L"left", 0).show.empty(), "do not unhide an app's own window");
    });
    cases.emplace_back("new_window_joins_selected_workspace", [] {
        WorkspaceModel model;
        auto first = Window(1, L"right"), second = Window(2, L"right");
        model.Observe(L"native-1", { first });
        Check(model.Commit(model.Step(L"native-1", L"right", 1)), "select second workspace");
        first.visible = false;
        model.Observe(L"native-1", { first, second });
        Check(model.Assignment(second.id) == 1, "new window follows current workspace");
    });
    cases.emplace_back("move_group_leaves_both_monitor_selections_unchanged", [] {
        WorkspaceModel model;
        auto left = Window(1, L"left"), right = Window(2, L"right"), dialog = Window(3, L"right");
        dialog.root = right.id;
        model.Observe(L"native-1", { dialog, left, right });
        auto move = model.Move(right.id, 1);
        Check(move.valid() && move.hide.size() == 2, "root and owned dialog move together");
        Check(!Has(move.hide, left.id), "left window untouched");
        Check(model.Commit(move), "commit move");
        Check(model.Assignment(right.id) == 1 && model.Assignment(dialog.id) == 1, "whole group assigned");
        Check(model.Selected(L"native-1", L"right") == 0, "moving does not switch");
    });
    cases.emplace_back("cross_monitor_owned_group_blocks_switch", [] {
        WorkspaceModel model;
        auto parent = Window(1, L"right"), dialog = Window(2, L"left");
        dialog.root = parent.id;
        model.Observe(L"native-1", { parent, dialog });
        auto change = model.Step(L"native-1", L"right", 1);
        Check(!change.valid() && change.rejection == Rejection::CrossMonitorGroup, "must not hide another monitor's dialog");
        Check(change.hide.empty() && change.show.empty(), "rejected plan contains no operations");
    });
    cases.emplace_back("uncommitted_transition_does_not_change_selection", [] {
        WorkspaceModel model;
        model.Observe(L"native-1", { Window(1, L"left") });
        const auto change = model.Step(L"native-1", L"left", 1);
        Check(change.valid(), "valid plan");
        Check(model.Selected(L"native-1", L"left") == 0, "planning cannot mutate state");
    });
    cases.emplace_back("stale_plan_cannot_commit", [] {
        WorkspaceModel model;
        auto window = Window(1, L"left");
        model.Observe(L"native-1", { window });
        auto change = model.Step(L"native-1", L"left", 1);
        model.Observe(L"native-1", { window });
        Check(!model.Commit(change), "stale operation rejected");
    });
    cases.emplace_back("native_desktop_selections_are_separate", [] {
        WorkspaceModel model;
        auto first = Window(1, L"left"), second = Window(2, L"left", L"native-2");
        model.Observe(L"native-1", { first, second });
        Check(!model.Assignment(second.id).has_value(), "inactive desktop is not adopted");
        Check(model.Commit(model.Step(L"native-1", L"left", 1)), "native one selection");
        model.ReleaseVisibility();
        model.Observe(L"native-2", { first, second });
        Check(model.Selected(L"native-2", L"left") == 0, "second native desktop starts at zero");
        auto change = model.Step(L"native-2", L"left", 1);
        Check(!Has(change.hide, first.id), "other native desktop cannot be touched");
    });
    cases.emplace_back("reused_handle_has_new_membership", [] {
        WorkspaceModel model;
        auto old = Window(1, L"left"), replacement = old;
        model.Observe(L"native-1", { old });
        Check(model.Commit(model.Step(L"native-1", L"left", 1)), "switch");
        replacement.id.created = 99;
        replacement.root = replacement.id;
        model.Observe(L"native-1", { replacement });
        Check(!model.Assignment(old.id), "old identity removed");
        Check(model.Assignment(replacement.id) == 1, "replacement belongs to current workspace");
        Check(model.Step(L"native-1", L"left", -1).show.empty(), "do not restore old handle");
    });
    cases.emplace_back("visible_window_moved_to_another_monitor_joins_its_current_workspace", [] {
        WorkspaceModel model;
        auto first = Window(1, L"left"), second = Window(2, L"right");
        model.Observe(L"native-1", { first, second });
        Check(model.Commit(model.Step(L"native-1", L"right", 1)), "right selects one");
        first.monitor = L"right";
        model.Observe(L"native-1", { first, second });
        Check(model.Assignment(first.id) == 1, "dragged window joins destination selection");
    });
    cases.emplace_back("workspace_wrap_and_invalid_selection", [] {
        WorkspaceModel model;
        model.Observe(L"native-1", { Window(1, L"left") });
        auto previous = model.Step(L"native-1", L"left", -1);
        Check(previous.selection == WorkspaceCount - 1, "wrap to last workspace");
        Check(!model.Select(L"native-1", L"left", WorkspaceCount).valid(), "out of range rejected");
    });
    cases.emplace_back("restore_resets_state", [] {
        WorkspaceModel model;
        model.Observe(L"native-1", { Window(1, L"left") });
        model.Commit(model.Step(L"native-1", L"left", 1));
        model.Reset();
        Check(model.size() == 0 && model.Selected(L"native-1", L"left") == 0, "reset clears windows and selections");
    });
    cases.emplace_back("capacity_exhaustion_refuses_visibility_changes", [] {
        WorkspaceModel model;
        std::vector<WindowSnapshot> windows;
        for (std::uint64_t i = 1; i <= MaxTrackedWindows + 1; ++i)
            windows.push_back(Window(i, L"left"));
        model.Observe(L"native-1", windows);
        auto change = model.Step(L"native-1", L"left", 1);
        Check(!change.valid() && change.rejection == Rejection::Capacity, "no partially managed desktop");
    });
    cases.emplace_back("window_marker_generation_prevents_same_process_handle_reuse", [] {
        WorkspaceModel model;
        auto old = Window(1, L"left"), replacement = old;
        old.id.incarnation = 1;
        old.root = old.id;
        model.Observe(L"native-1", { old });
        Check(model.Commit(model.Step(L"native-1", L"left", 1)), "switch old window");
        replacement.id.incarnation = 2;
        replacement.root = replacement.id;
        model.Observe(L"native-1", { replacement });
        Check(!model.Assignment(old.id), "same PID, thread and HWND with new marker is distinct");
        Check(model.Assignment(replacement.id) == 1, "replacement joins selected workspace");
    });
    cases.emplace_back("fixed_seed_10000_transitions_preserve_independent_monitors", [] {
        WorkspaceModel model;
        std::vector<WindowSnapshot> windows;
        for (std::uint64_t i = 1; i <= 20; ++i)
            windows.push_back(Window(i, i <= 10 ? L"left" : L"right"));
        std::mt19937 random(49420);
        unsigned selected[2]{};
        unsigned assigned[20]{};
        for (unsigned step = 0; step < 10000; ++step)
        {
            model.Observe(L"native-1", windows);
            const unsigned monitorIndex = random() % 2;
            const std::wstring monitor = monitorIndex == 0 ? L"left" : L"right";
            const int direction = random() % 2 == 0 ? -1 : 1;
            const unsigned target = direction < 0 ? (selected[monitorIndex] + WorkspaceCount - 1) % WorkspaceCount : (selected[monitorIndex] + 1) % WorkspaceCount;
            auto plan = model.Step(L"native-1", monitor, direction);
            Check(plan.valid(), "generated switch is valid");
            Check(model.Selected(L"native-1", monitor) == selected[monitorIndex], "plan is nonmutating");
            for (unsigned i = 0; i < windows.size(); ++i)
            {
                const bool onMonitor = (i < 10 ? 0U : 1U) == monitorIndex;
                const bool nextVisible = onMonitor ? assigned[i] == target : windows[i].visible;
                Check(Has(plan.hide, windows[i].id) == (windows[i].visible && !nextVisible), "hide set equals reference model");
                Check(Has(plan.show, windows[i].id) == (!windows[i].visible && nextVisible), "show set equals reference model");
                windows[i].visible = nextVisible;
            }
            Check(model.Commit(plan), "commit generated switch");
            selected[monitorIndex] = target;
            Check(model.Selected(L"native-1", L"left") == selected[0] && model.Selected(L"native-1", L"right") == selected[1], "both selections match independent reference");
        }
    });
    cases.emplace_back("app_hidden_cross_monitor_dialog_still_blocks_group_switch", [] {
        WorkspaceModel model;
        auto root = Window(1, L"right"), dialog = Window(2, L"left");
        dialog.root = root.id;
        model.Observe(L"native-1", { root, dialog });
        dialog.visible = false; // The app hid its own dialog, not the utility.
        model.Observe(L"native-1", { root, dialog });
        const auto plan = model.Step(L"native-1", L"right", 1);
        Check(!plan.valid() && plan.rejection == Rejection::CrossMonitorGroup, "entire owned group must stay on one monitor");
        Check(plan.hide.empty() && plan.show.empty(), "refused switch cannot hide the root");
        Check(!model.Move(root.id, 1).valid(), "moving that group must also be refused");
    });
    return cases;
}
