// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.
using Microsoft.PowerToys.Settings.UI.Helpers;
using Microsoft.PowerToys.Settings.UI.Library;
using Microsoft.PowerToys.Settings.UI.ViewModels;
using Microsoft.UI.Xaml.Controls;

namespace Microsoft.PowerToys.Settings.UI.Views
{
    public sealed partial class IndependentDesktopsPage : NavigablePage, IRefreshablePage
    {
        private IndependentDesktopsViewModel ViewModel { get; }

        public IndependentDesktopsPage()
        {
            var settingsUtils = SettingsUtils.Default;
            ViewModel = new IndependentDesktopsViewModel(settingsUtils, SettingsRepository<GeneralSettings>.GetInstance(settingsUtils), SettingsRepository<IndependentDesktopsSettings>.GetInstance(settingsUtils), ShellPage.SendDefaultIPCMessage);
            DataContext = ViewModel;
            InitializeComponent();
            Loaded += (_, _) => ViewModel.OnPageLoaded();
        }

        public void RefreshEnabledState() => ViewModel.RefreshEnabledState();
    }
}
