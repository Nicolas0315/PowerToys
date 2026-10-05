// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.
#pragma once

#include <common/utils/json.h>

namespace RunnerSettingsDispatch
{
    inline bool GetHotkeyChanged(const json::JsonValue& moduleSettings) noexcept
    {
        try
        {
            if (moduleSettings.ValueType() != json::JsonValueType::Object)
            {
                return true;
            }

            const auto settings = moduleSettings.GetObjectW();
            if (!settings.HasKey(L"properties"))
            {
                return true;
            }

            const auto properties = settings.GetNamedValue(L"properties");
            if (properties.ValueType() != json::JsonValueType::Object)
            {
                return true;
            }

            const auto propertiesObject = properties.GetObjectW();
            if (!propertiesObject.HasKey(L"hotkey_changed"))
            {
                return true;
            }

            const auto hotkeyChanged = propertiesObject.GetNamedValue(L"hotkey_changed");
            return hotkeyChanged.ValueType() == json::JsonValueType::Boolean ? hotkeyChanged.GetBoolean() : true;
        }
        catch (...)
        {
            return true;
        }
    }

    template<typename Receiver>
    void DispatchModuleSettings(const json::JsonObject& modules, Receiver&& receiver)
    {
        for (const auto& module : modules)
        {
            const auto value = module.Value();
            const std::wstring serializedValue{ value.Stringify().c_str() };
            receiver(module.Key().c_str(), serializedValue, GetHotkeyChanged(value));
        }
    }
}
