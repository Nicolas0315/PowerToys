// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

using System.Text.Json.Serialization;
using Settings.UI.Library.Attributes;

namespace Microsoft.PowerToys.Settings.UI.Library
{
    public class IndependentDesktopsProperties
    {
        private const int Left = 0x25;
        private const int Right = 0x27;
        private const int Home = 0x24;

        [JsonIgnore]
        [CmdConfigureIgnore]
        public HotkeySettings DefaultPreviousDesktopShortcut => new HotkeySettings(true, true, true, false, Left);

        [JsonIgnore]
        [CmdConfigureIgnore]
        public HotkeySettings DefaultNextDesktopShortcut => new HotkeySettings(true, true, true, false, Right);

        [JsonIgnore]
        [CmdConfigureIgnore]
        public HotkeySettings DefaultMovePreviousDesktopShortcut => new HotkeySettings(true, true, true, true, Left);

        [JsonIgnore]
        [CmdConfigureIgnore]
        public HotkeySettings DefaultMoveNextDesktopShortcut => new HotkeySettings(true, true, true, true, Right);

        [JsonIgnore]
        [CmdConfigureIgnore]
        public HotkeySettings DefaultRestoreWindowsShortcut => new HotkeySettings(true, true, true, false, Home);

        private HotkeySettings previousDesktopShortcut;
        private HotkeySettings nextDesktopShortcut;
        private HotkeySettings movePreviousDesktopShortcut;
        private HotkeySettings moveNextDesktopShortcut;
        private HotkeySettings restoreWindowsShortcut;

        [JsonPropertyName("previous_desktop")]
        public HotkeySettings PreviousDesktopShortcut { get => previousDesktopShortcut ?? DefaultPreviousDesktopShortcut; set => previousDesktopShortcut = value; }

        [JsonPropertyName("next_desktop")]
        public HotkeySettings NextDesktopShortcut { get => nextDesktopShortcut ?? DefaultNextDesktopShortcut; set => nextDesktopShortcut = value; }

        [JsonPropertyName("move_previous_desktop")]
        public HotkeySettings MovePreviousDesktopShortcut { get => movePreviousDesktopShortcut ?? DefaultMovePreviousDesktopShortcut; set => movePreviousDesktopShortcut = value; }

        [JsonPropertyName("move_next_desktop")]
        public HotkeySettings MoveNextDesktopShortcut { get => moveNextDesktopShortcut ?? DefaultMoveNextDesktopShortcut; set => moveNextDesktopShortcut = value; }

        [JsonPropertyName("restore_windows")]
        public HotkeySettings RestoreWindowsShortcut { get => restoreWindowsShortcut ?? DefaultRestoreWindowsShortcut; set => restoreWindowsShortcut = value; }

        public IndependentDesktopsProperties()
        {
            PreviousDesktopShortcut = DefaultPreviousDesktopShortcut;
            NextDesktopShortcut = DefaultNextDesktopShortcut;
            MovePreviousDesktopShortcut = DefaultMovePreviousDesktopShortcut;
            MoveNextDesktopShortcut = DefaultMoveNextDesktopShortcut;
            RestoreWindowsShortcut = DefaultRestoreWindowsShortcut;
        }
    }
}
