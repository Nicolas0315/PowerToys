// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.
#include "CoreCases.h"
int main()
{
    unsigned failures = 0;
    for (const auto& [name, run] : CoreCases())
    {
        try { run(); std::cout << "PASS " << name << '\n'; }
        catch (const std::exception& error) { ++failures; std::cout << "FAIL " << name << ": " << error.what() << '\n'; }
    }
    std::cout << "RESULT failures=" << failures << '\n';
    return failures == 0 ? 0 : 1;
}
