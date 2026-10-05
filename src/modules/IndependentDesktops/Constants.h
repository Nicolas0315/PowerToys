// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.
#pragma once

#include <cstddef>

namespace IndependentDesktops
{
    inline constexpr unsigned WorkspaceCount = 4;
    inline constexpr std::size_t MaxTrackedWindows = 1024;
    inline constexpr wchar_t ModuleKey[] = L"IndependentDesktops";
    inline constexpr wchar_t ExecutableName[] = L"PowerToys.IndependentDesktops.exe";
    enum class Command : unsigned { Previous, Next, MovePrevious, MoveNext, Restore, Stop, Count };
    inline constexpr const wchar_t* EventNames[] = {
        L"Local\\PowerToys_IndependentDesktops_Previous_6EAF3B79",
        L"Local\\PowerToys_IndependentDesktops_Next_6EAF3B79",
        L"Local\\PowerToys_IndependentDesktops_MovePrevious_6EAF3B79",
        L"Local\\PowerToys_IndependentDesktops_MoveNext_6EAF3B79",
        L"Local\\PowerToys_IndependentDesktops_Restore_6EAF3B79",
        L"Local\\PowerToys_IndependentDesktops_Stop_6EAF3B79" };
    inline constexpr const wchar_t* ShortcutKeys[] = {
        L"previous_desktop", L"next_desktop", L"move_previous_desktop", L"move_next_desktop", L"restore_windows" };
}
