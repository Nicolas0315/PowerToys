// Copyright (c) Microsoft Corporation
// The Microsoft Corporation licenses this file to you under the MIT license.
// See the LICENSE file in the project root for more information.

using System;
using System.Collections.Generic;
using System.Globalization;
using System.Text.Json;
using global::PowerToys.GPOWrapper;
using Microsoft.PowerToys.Settings.UI.Helpers;
using Microsoft.PowerToys.Settings.UI.Library;
using Microsoft.PowerToys.Settings.UI.Library.Interfaces;
using Microsoft.PowerToys.Settings.UI.SerializationContext;

namespace Microsoft.PowerToys.Settings.UI.ViewModels
{
    public partial class IndependentDesktopsViewModel : PageViewModelBase
    {
        protected override string ModuleName => IndependentDesktopsSettings.ModuleName;

        private readonly SettingsUtils settingsUtils;
        private readonly GeneralSettings generalSettingsConfig;
        private readonly IndependentDesktopsSettings settings;
        private readonly Func<string, int> sendConfigMessage;
        private GpoRuleConfigured enabledGpoRuleConfiguration;
        private bool enabledStateIsGpoConfigured;
        private bool isEnabled;
        private HotkeySettings previousDesktopShortcut;
        private HotkeySettings nextDesktopShortcut;
        private HotkeySettings movePreviousDesktopShortcut;
        private HotkeySettings moveNextDesktopShortcut;
        private HotkeySettings restoreWindowsShortcut;

        public IndependentDesktopsViewModel(SettingsUtils settingsUtils, ISettingsRepository<GeneralSettings> generalSettingsRepository, ISettingsRepository<IndependentDesktopsSettings> moduleSettingsRepository, Func<string, int> ipcMessageCallback)
        {
            this.settingsUtils = settingsUtils ?? throw new ArgumentNullException(nameof(settingsUtils));
            generalSettingsConfig = (generalSettingsRepository ?? throw new ArgumentNullException(nameof(generalSettingsRepository))).SettingsConfig;
            settings = (moduleSettingsRepository ?? throw new ArgumentNullException(nameof(moduleSettingsRepository))).SettingsConfig;
            sendConfigMessage = ipcMessageCallback;
            InitializeEnabledValue();
            previousDesktopShortcut = settings.Properties.PreviousDesktopShortcut;
            nextDesktopShortcut = settings.Properties.NextDesktopShortcut;
            movePreviousDesktopShortcut = settings.Properties.MovePreviousDesktopShortcut;
            moveNextDesktopShortcut = settings.Properties.MoveNextDesktopShortcut;
            restoreWindowsShortcut = settings.Properties.RestoreWindowsShortcut;
        }

        public override Dictionary<string, HotkeySettings[]> GetAllHotkeySettings() => new()
        {
            [ModuleName] = [PreviousDesktopShortcut, NextDesktopShortcut, MovePreviousDesktopShortcut, MoveNextDesktopShortcut, RestoreWindowsShortcut],
        };

        public bool IsEnabled
        {
            get => isEnabled;
            set
            {
                if (enabledStateIsGpoConfigured || value == isEnabled)
                {
                    return;
                }

                isEnabled = value;
                generalSettingsConfig.Enabled.IndependentDesktops = value;
                sendConfigMessage(new OutGoingGeneralSettings(generalSettingsConfig).ToString());
                OnPropertyChanged();
            }
        }

        public bool IsEnabledGpoConfigured => enabledStateIsGpoConfigured;

        public HotkeySettings PreviousDesktopShortcut { get => previousDesktopShortcut; set => UpdateShortcut(ref previousDesktopShortcut, value, settings.Properties.DefaultPreviousDesktopShortcut, shortcut => settings.Properties.PreviousDesktopShortcut = shortcut, nameof(PreviousDesktopShortcut)); }

        public HotkeySettings NextDesktopShortcut { get => nextDesktopShortcut; set => UpdateShortcut(ref nextDesktopShortcut, value, settings.Properties.DefaultNextDesktopShortcut, shortcut => settings.Properties.NextDesktopShortcut = shortcut, nameof(NextDesktopShortcut)); }

        public HotkeySettings MovePreviousDesktopShortcut { get => movePreviousDesktopShortcut; set => UpdateShortcut(ref movePreviousDesktopShortcut, value, settings.Properties.DefaultMovePreviousDesktopShortcut, shortcut => settings.Properties.MovePreviousDesktopShortcut = shortcut, nameof(MovePreviousDesktopShortcut)); }

        public HotkeySettings MoveNextDesktopShortcut { get => moveNextDesktopShortcut; set => UpdateShortcut(ref moveNextDesktopShortcut, value, settings.Properties.DefaultMoveNextDesktopShortcut, shortcut => settings.Properties.MoveNextDesktopShortcut = shortcut, nameof(MoveNextDesktopShortcut)); }

        public HotkeySettings RestoreWindowsShortcut { get => restoreWindowsShortcut; set => UpdateShortcut(ref restoreWindowsShortcut, value, settings.Properties.DefaultRestoreWindowsShortcut, shortcut => settings.Properties.RestoreWindowsShortcut = shortcut, nameof(RestoreWindowsShortcut)); }

        public void RefreshEnabledState()
        {
            InitializeEnabledValue();
            OnPropertyChanged(nameof(IsEnabled));
        }

        private void InitializeEnabledValue()
        {
            enabledGpoRuleConfiguration = GPOWrapper.GetConfiguredIndependentDesktopsEnabledValue();
            enabledStateIsGpoConfigured = enabledGpoRuleConfiguration is GpoRuleConfigured.Enabled or GpoRuleConfigured.Disabled;
            isEnabled = enabledStateIsGpoConfigured ? enabledGpoRuleConfiguration == GpoRuleConfigured.Enabled : generalSettingsConfig.Enabled.IndependentDesktops;
        }

        private void UpdateShortcut(ref HotkeySettings field, HotkeySettings value, HotkeySettings fallback, Action<HotkeySettings> save, string propertyName)
        {
            value ??= fallback;
            if (value == field)
            {
                return;
            }

            field = value;
            save(value);
            OnPropertyChanged(propertyName);
            settingsUtils.SaveSettings(settings.ToJsonString(), ModuleName);
            sendConfigMessage(string.Format(CultureInfo.InvariantCulture, "{{ \"powertoys\": {{ \"{0}\": {1} }} }}", ModuleName, JsonSerializer.Serialize(settings, SourceGenerationContextContext.Default.IndependentDesktopsSettings)));
        }
    }
}
