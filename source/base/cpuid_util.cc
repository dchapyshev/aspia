//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/cpuid_util.h"

#if defined(Q_PROCESSOR_X86)

#include "base/bitset.h"

#if defined(Q_CC_MSVC)
#include <intrin.h>
#else
#include <cpuid.h>
#endif // Q_CC_MSVC

namespace base {

//--------------------------------------------------------------------------------------------------
CpuidUtil::CpuidUtil(int leaf, int subleaf)
{
    get(leaf, subleaf);
}

//--------------------------------------------------------------------------------------------------
CpuidUtil::CpuidUtil(const CpuidUtil& other)
{
    *this = other;
}

//--------------------------------------------------------------------------------------------------
CpuidUtil& CpuidUtil::operator=(const CpuidUtil& other)
{
    if (&other == this)
        return *this;

    eax_ = other.eax_;
    ebx_ = other.ebx_;
    ecx_ = other.ecx_;
    edx_ = other.edx_;
    return *this;
}

//--------------------------------------------------------------------------------------------------
void CpuidUtil::get(int leaf, int subleaf)
{
#if defined(Q_CC_MSVC)
    int cpu_info[4];
    __cpuidex(cpu_info, leaf, subleaf);
#else
    unsigned int cpu_info[4];
    __get_cpuid_count(leaf, subleaf, &cpu_info[0], &cpu_info[1], &cpu_info[2], &cpu_info[3]);
#endif

    eax_ = static_cast<quint32>(cpu_info[0]);
    ebx_ = static_cast<quint32>(cpu_info[1]);
    ecx_ = static_cast<quint32>(cpu_info[2]);
    edx_ = static_cast<quint32>(cpu_info[3]);
}

//--------------------------------------------------------------------------------------------------
// static
bool CpuidUtil::hasAesNi()
{
    // Check if function 1 is supported.
    if (CpuidUtil(0).eax() < 1)
        return false;

    // Bit 25 of register ECX set to 1 indicates the support of AES instructions.
    return BitSet<quint32>(CpuidUtil(1).ecx()).test(25);
}

} // namespace base

#endif // defined(Q_PROCESSOR_X86)
