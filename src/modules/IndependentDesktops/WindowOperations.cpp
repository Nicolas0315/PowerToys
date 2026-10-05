// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.
#include "WindowOperations.h"
namespace IndependentDesktops
{
    std::optional<WindowId> IdentifyWindow(HWND) { return {}; }
    bool MatchesWindow(const WindowId&, bool) { return false; }
    bool WaitForVisibility(const WindowId&, bool, DWORD) { return false; }
    std::wstring MonitorName(HMONITOR) { return {}; }
}
