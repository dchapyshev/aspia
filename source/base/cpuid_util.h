//
// Aspia Project
// Copyright (C) 2016-2024 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef BASE_CPUID_UTIL_H
#define BASE_CPUID_UTIL_H

#include "build/build_config.h"

#if defined(ARCH_CPU_X86_FAMILY)

#include <cstdint>
#if !defined(CC_MSVC)
#include <climits>
#endif

namespace base {

class CpuidUtil
{
public:
    CpuidUtil() = default;
    CpuidUtil(int leaf, int subleaf = 0);
    virtual ~CpuidUtil() = default;

    CpuidUtil(const CpuidUtil& other);
    CpuidUtil& operator=(const CpuidUtil& other);

    void get(int leaf, int subleaf = 0);

    uint32_t eax() const { return eax_; }
    uint32_t ebx() const { return ebx_; }
    uint32_t ecx() const { return ecx_; }
    uint32_t edx() const { return edx_; }

    static bool hasAesNi();

private:
    uint32_t eax_ = 0;
    uint32_t ebx_ = 0;
    uint32_t ecx_ = 0;
    uint32_t edx_ = 0;
};

} // namespace base

#endif // defined(ARCH_CPU_X86_FAMILY)
#endif // BASE_CPUID_UTIL_H
