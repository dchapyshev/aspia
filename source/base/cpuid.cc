//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#include "base/cpuid.h"

#include <cstring>

#include "base/bitset.h"

namespace aspia {

CPUID::CPUID(const CPUID& other)
{
    memcpy(cpu_info_, other.cpu_info_, sizeof(cpu_info_));
}

CPUID& CPUID::operator=(const CPUID& other)
{
    if (&other == this)
        return *this;

    memcpy(cpu_info_, other.cpu_info_, sizeof(cpu_info_));
    return *this;
}

void CPUID::get(int leaf)
{
    #ifdef CC_MSVC
    __cpuid(cpu_info_, leaf);
    #endif
}

void CPUID::get(int leaf, int subleaf)
{
    #ifdef CC_MSVC
    __cpuidex(cpu_info_, leaf, subleaf);
    #endif
}

// static
bool CPUID::hasAesNi()
{
    unsigned int b;

    #ifdef CC_GCC
    //TODO: perform actual check
    b = 0;
    #endif
    #ifdef CC_MSVC
    __asm
    {
        mov     eax, 1
        cpuid
        mov     b, ecx
    }
    #endif

    return (b & (1 << 25)) != 0;
}

} // namespace aspia
