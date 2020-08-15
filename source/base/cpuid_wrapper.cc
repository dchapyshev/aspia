//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/compiler_specific.h"
#include "base/cpuid_wrapper.h"
#include "base/bitset.h"

#if defined(CC_MSVC)
#include <intrin.h>
#else
#include <cpuid.h>
#endif // CC_MSVC
#include <cstring>

namespace base {

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

#if defined(CC_MSVC)

void CPUID::get(int leaf)
{
    __cpuid(cpu_info_, leaf);
}

void CPUID::get(int leaf, int subleaf)
{
    __cpuidex(cpu_info_, leaf, subleaf);
}

#else

void CPUID::get(int leaf)
{
    __get_cpuid(leaf, &cpu_info_[0], &cpu_info_[1], &cpu_info_[2], &cpu_info_[3]);
}

void CPUID::get(int leaf, int subleaf)
{
    __get_cpuid_count(leaf, subleaf, & cpu_info_[0], &cpu_info_[1], &cpu_info_[2], &cpu_info_[3]);
}

#endif // CC_MSVC

// static
bool CPUID::hasAesNi()
{
    // Check if function 1 is supported.
    if (CPUID(0).eax() < 1)
        return false;

    // Bit 25 of register ECX set to 1 indicates the support of AES instructions.
    return BitSet<uint32_t>(CPUID(1).ecx()).test(25);
}

} // namespace base
