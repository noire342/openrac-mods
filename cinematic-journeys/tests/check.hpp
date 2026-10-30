// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <stdexcept>
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition))                                                                          \
            throw std::runtime_error(#condition);                                                  \
    } while (0)
