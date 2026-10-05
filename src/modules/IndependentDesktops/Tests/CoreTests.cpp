// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.
#include "../WorkspaceModel.h"
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace IndependentDesktops;
static void Check(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}
static WindowSnapshot Window(std::uint64_t id, const wchar_t* monitor, const wchar_t* desktop = L"native-1", bool visible = true)
{
    WindowId identity{ id, 10, 20, 30 };
    return { identity, identity, monitor, desktop, visible, true };
}
static bool Has(const std::vector<WindowId>& values, const WindowId& id)
{
    for (const auto& value : values) if (value == id) return true;
    return false;
}
int main()
{
    unsigned failures = 0;
    const auto test = [&](const char* name, const std::function<void()>& run) {
        try { run(); std::cout << "PASS " << name << '\n'; }
        catch (const std::exception& error) { ++failures; std::cout << "FAIL " << name << ": " << error.what() << '\n'; }
    };
    test("switch_only_target_monitor", [] {
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
    test("switch_back_restores_only_utility_hidden_windows", [] {
        WorkspaceModel model;
        auto window = Window(1, L"left");
        model.Observe(L"native-1", { window });
        Check(model.Commit(model.Step(L"native-1", L"left", 1)), "first switch");
        window.visible = false;
        model.Observe(L"native-1", { window });
        auto back = model.Step(L"native-1", L"left", -1);
        Check(back.valid() && Has(back.show, window.id), "utility hidden window must return");
    });
    test("hidden_windows_are_not_adopted", [] {
        WorkspaceModel model;
        model.Observe(L"native-1", { Window(1, L"left", L"native-1", false) });
        Check(model.size() == 0, "app-hidden window must stay unmanaged");
    });
    test("application_hidden_window_is_not_shown", [] {
        WorkspaceModel model;
        auto window = Window(1, L"left");
        model.Observe(L"native-1", { window });
        window.visible = false;
        model.Observe(L"native-1", { window });
        Check(model.Select(L"native-1", L"left", 0).show.empty(), "do not unhide an app's own window");
    });
    test("new_window_joins_selected_workspace", [] {
        WorkspaceModel model;
        auto first = Window(1, L"right"), second = Window(2, L"right");
        model.Observe(L"native-1", { first });
        Check(model.Commit(model.Step(L"native-1", L"right", 1)), "select second workspace");
        first.visible = false;
        model.Observe(L"native-1", { first, second });
        Check(model.Assignment(second.id) == 1, "new window follows current workspace");
    });
    test("move_group_leaves_both_monitor_selections_unchanged", [] {
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
    test("cross_monitor_owned_group_blocks_switch", [] {
        WorkspaceModel model;
        auto parent = Window(1, L"right"), dialog = Window(2, L"left");
        dialog.root = parent.id;
        model.Observe(L"native-1", { parent, dialog });
        auto change = model.Step(L"native-1", L"right", 1);
        Check(!change.valid() && change.rejection == Rejection::CrossMonitorGroup, "must not hide another monitor's dialog");
        Check(change.hide.empty() && change.show.empty(), "rejected plan contains no operations");
    });
    test("uncommitted_transition_does_not_change_selection", [] {
        WorkspaceModel model;
        model.Observe(L"native-1", { Window(1, L"left") });
        const auto change = model.Step(L"native-1", L"left", 1);
        Check(change.valid(), "valid plan");
        Check(model.Selected(L"native-1", L"left") == 0, "planning cannot mutate state");
    });
    test("stale_plan_cannot_commit", [] {
        WorkspaceModel model;
        auto window = Window(1, L"left");
        model.Observe(L"native-1", { window });
        auto change = model.Step(L"native-1", L"left", 1);
        model.Observe(L"native-1", { window });
        Check(!model.Commit(change), "stale operation rejected");
    });
    test("native_desktop_selections_are_separate", [] {
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
    test("reused_handle_has_new_membership", [] {
        WorkspaceModel model;
        auto old = Window(1, L"left"), replacement = old;
        model.Observe(L"native-1", { old });
        Check(model.Commit(model.Step(L"native-1", L"left", 1)), "switch");
        replacement.id.created = 99; replacement.root = replacement.id;
        model.Observe(L"native-1", { replacement });
        Check(!model.Assignment(old.id), "old identity removed");
        Check(model.Assignment(replacement.id) == 1, "replacement belongs to current workspace");
        Check(model.Step(L"native-1", L"left", -1).show.empty(), "do not restore old handle");
    });
    test("visible_window_moved_to_another_monitor_joins_its_current_workspace", [] {
        WorkspaceModel model;
        auto first = Window(1, L"left"), second = Window(2, L"right");
        model.Observe(L"native-1", { first, second });
        Check(model.Commit(model.Step(L"native-1", L"right", 1)), "right selects one");
        first.monitor = L"right";
        model.Observe(L"native-1", { first, second });
        Check(model.Assignment(first.id) == 1, "dragged window joins destination selection");
    });
    test("workspace_wrap_and_invalid_selection", [] {
        WorkspaceModel model;
        model.Observe(L"native-1", { Window(1, L"left") });
        auto previous = model.Step(L"native-1", L"left", -1);
        Check(previous.selection == WorkspaceCount - 1, "wrap to last workspace");
        Check(!model.Select(L"native-1", L"left", WorkspaceCount).valid(), "out of range rejected");
    });
    test("restore_resets_state", [] {
        WorkspaceModel model;
        model.Observe(L"native-1", { Window(1, L"left") });
        model.Commit(model.Step(L"native-1", L"left", 1));
        model.Reset();
        Check(model.size() == 0 && model.Selected(L"native-1", L"left") == 0, "reset clears windows and selections");
    });
    test("capacity_exhaustion_refuses_visibility_changes", [] {
        WorkspaceModel model;
        std::vector<WindowSnapshot> windows;
        for (std::uint64_t i = 1; i <= MaxTrackedWindows + 1; ++i) windows.push_back(Window(i, L"left"));
        model.Observe(L"native-1", windows);
        auto change = model.Step(L"native-1", L"left", 1);
        Check(!change.valid() && change.rejection == Rejection::Capacity, "no partially managed desktop");
    });
    std::cout << "RESULT failures=" << failures << '\n';
    return failures == 0 ? 0 : 1;
}
