// Copyright (c) Microsoft Corporation
// The Microsoft Corporation licenses this file to you under the MIT license.
// See the LICENSE file in the project root for more information.

using System.Collections.Generic;
using System.Text.Json.Serialization;
using ManagedCommon;
using Microsoft.PowerToys.Settings.UI.Library.Helpers;
using Microsoft.PowerToys.Settings.UI.Library.Interfaces;

namespace Microsoft.PowerToys.Settings.UI.Library
{
    public class IndependentDesktopsSettings : BasePTModuleSettings, ISettingsConfig, IHotkeyConfig
    {
        public const string ModuleName = "IndependentDesktops";

        [JsonPropertyName("properties")]
        public IndependentDesktopsProperties Properties { get; set; }

        public IndependentDesktopsSettings()
        {
            Name = ModuleName;
            Version = "1.0";
            Properties = new IndependentDesktopsProperties();
        }

        public string GetModuleName() => Name;

        public ModuleType GetModuleType() => ModuleType.IndependentDesktops;

        public HotkeyAccessor[] GetAllHotkeyAccessors()
        {
            return new List<HotkeyAccessor>
            {
                new(() => Properties.PreviousDesktopShortcut, value => Properties.PreviousDesktopShortcut = value ?? Properties.DefaultPreviousDesktopShortcut, "IndependentDesktops_PreviousDesktopShortcut"),
                new(() => Properties.NextDesktopShortcut, value => Properties.NextDesktopShortcut = value ?? Properties.DefaultNextDesktopShortcut, "IndependentDesktops_NextDesktopShortcut"),
                new(() => Properties.MovePreviousDesktopShortcut, value => Properties.MovePreviousDesktopShortcut = value ?? Properties.DefaultMovePreviousDesktopShortcut, "IndependentDesktops_MovePreviousDesktopShortcut"),
                new(() => Properties.MoveNextDesktopShortcut, value => Properties.MoveNextDesktopShortcut = value ?? Properties.DefaultMoveNextDesktopShortcut, "IndependentDesktops_MoveNextDesktopShortcut"),
                new(() => Properties.RestoreWindowsShortcut, value => Properties.RestoreWindowsShortcut = value ?? Properties.DefaultRestoreWindowsShortcut, "IndependentDesktops_RestoreWindowsShortcut"),
            }.ToArray();
        }

        public bool UpgradeSettingsConfiguration() => false;
    }
}
