// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.
#pragma once

#include <memory>
#include <utility>

namespace RunnerSettingsDispatch
{
    template<typename Message, typename Receiver>
    bool DispatchOwned(Message* raw, Receiver&& receiver) noexcept
    {
        std::unique_ptr<Message> message(raw);
        if (!message)
        {
            return false;
        }

        try
        {
            std::forward<Receiver>(receiver)(*message);
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

}
