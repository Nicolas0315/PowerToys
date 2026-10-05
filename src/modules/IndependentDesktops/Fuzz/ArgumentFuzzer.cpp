// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.
#include "../Recovery.h"
#include <cstdlib>
#include <limits>
#include <string>

// Only exercises number parsing. It never opens an input handle or operates a window.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size)
{
    if (size > 64)
        return 0;
    std::wstring input;
    for (std::size_t i = 0; i < size && data[i] != 0; ++i)
        input.push_back(static_cast<wchar_t>(data[i]));
    bool expected = !input.empty();
    ULONG_PTR value{};
    constexpr ULONG_PTR maximum = std::numeric_limits<ULONG_PTR>::max();
    for (const auto character : input)
    {
        if (character < L'0' || character > L'9')
        {
            expected = false;
            break;
        }
        const auto digit = static_cast<ULONG_PTR>(character - L'0');
        if (value > (maximum - digit) / 10)
        {
            expected = false;
            break;
        }
        value = value * 10 + digit;
    }
    expected = expected && value != 0 && value != maximum;
    HANDLE actual{};
    const bool parsed = IndependentDesktops::ParseHandle(input.c_str(), actual);
    if (parsed != expected || (parsed && reinterpret_cast<ULONG_PTR>(actual) != value))
        std::abort();
    return 0;
}
