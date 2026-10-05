// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.
using System.Text.Json;
using Microsoft.PowerToys.Settings.UI.Library;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace CommonLibTest
{
    [TestClass]
    public class IndependentDesktopsSettingsTests
    {
        [TestMethod]
        public void Defaults_ShouldUseDocumentedShortcuts()
        {
            var settings = new IndependentDesktopsSettings();
            Assert.AreEqual(0x25, settings.Properties.PreviousDesktopShortcut.Code);
            Assert.AreEqual(0x27, settings.Properties.NextDesktopShortcut.Code);
            Assert.IsTrue(settings.Properties.MovePreviousDesktopShortcut.Shift);
            Assert.IsTrue(settings.Properties.MoveNextDesktopShortcut.Shift);
            Assert.AreEqual(0x24, settings.Properties.RestoreWindowsShortcut.Code);
            Assert.IsTrue(settings.Properties.RestoreWindowsShortcut.Win);
            Assert.IsTrue(settings.Properties.RestoreWindowsShortcut.Ctrl);
            Assert.IsTrue(settings.Properties.RestoreWindowsShortcut.Alt);
        }

        [TestMethod]
        public void RoundTrip_ShouldPreserveAllShortcutKeys()
        {
            var json = new IndependentDesktopsSettings().ToJsonString();
            StringAssert.Contains(json, "previous_desktop");
            StringAssert.Contains(json, "next_desktop");
            StringAssert.Contains(json, "move_previous_desktop");
            StringAssert.Contains(json, "move_next_desktop");
            StringAssert.Contains(json, "restore_windows");
            var roundTrip = JsonSerializer.Deserialize<IndependentDesktopsSettings>(json);
            Assert.IsNotNull(roundTrip);
            Assert.AreEqual(0x24, roundTrip.Properties.RestoreWindowsShortcut.Code);
        }

        [TestMethod]
        public void HotkeyRegistration_ShouldUseAllFiveRunnerCommandsInOrder()
        {
            var hotkeys = new IndependentDesktopsSettings().GetAllHotkeyAccessors();

            Assert.HasCount(5, hotkeys);
            CollectionAssert.AreEqual(
                new[]
                {
                    "IndependentDesktops_PreviousDesktopShortcut",
                    "IndependentDesktops_NextDesktopShortcut",
                    "IndependentDesktops_MovePreviousDesktopShortcut",
                    "IndependentDesktops_MoveNextDesktopShortcut",
                    "IndependentDesktops_RestoreWindowsShortcut",
                },
                System.Array.ConvertAll(hotkeys, accessor => accessor.LocalizationHeaderKey));
        }

        [TestMethod]
        public void SettingsSerializationContext_ShouldRegisterModuleSettings()
        {
            var options = new JsonSerializerOptions { TypeInfoResolver = SettingsSerializationContext.Default };
            Assert.IsNotNull(options.TypeInfoResolver.GetTypeInfo(typeof(IndependentDesktopsSettings), options));
        }
    }
}
