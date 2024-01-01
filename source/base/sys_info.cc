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

#include "base/sys_info.h"

#include "base/cpuid_util.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/unicode.h"

#include <algorithm>
#include <cstring>

namespace base {

//--------------------------------------------------------------------------------------------------
// static
std::u16string SysInfo::processorName()
{
#if defined(ARCH_CPU_X86_FAMILY)
    CpuidUtil cpuidUtil;
    cpuidUtil.get(static_cast<int>(0x80000000));

    uint32_t max_leaf = cpuidUtil.eax();
    if (max_leaf < 0x80000002)
        return std::u16string();

    max_leaf = std::min(max_leaf, 0x80000004);

    char buffer[49];
    memset(&buffer[0], 0, sizeof(buffer));

    for (uint32_t leaf = 0x80000002, offset = 0; leaf <= max_leaf; ++leaf, offset += 16)
    {
        cpuidUtil.get(static_cast<int>(leaf));

        uint32_t eax = cpuidUtil.eax();
        uint32_t ebx = cpuidUtil.ebx();
        uint32_t ecx = cpuidUtil.ecx();
        uint32_t edx = cpuidUtil.edx();

        memcpy(&buffer[offset + 0], &eax, sizeof(eax));
        memcpy(&buffer[offset + 4], &ebx, sizeof(ebx));
        memcpy(&buffer[offset + 8], &ecx, sizeof(ecx));
        memcpy(&buffer[offset + 12], &edx, sizeof(edx));
    }

    std::u16string result = utf16FromAscii(buffer);

    removeChars(&result, u"(TM)");
    removeChars(&result, u"(tm)");
    removeChars(&result, u"(R)");
    removeChars(&result, u"CPU");
    removeChars(&result, u"Quad-Core Processor");
    removeChars(&result, u"Six-Core Processor");
    removeChars(&result, u"Eight-Core Processor");

    std::u16string_view at(u"@");
    auto sub = find_end(result.begin(), result.end(), at.begin(), at.end());
    if (sub != result.end())
        result.erase(sub - 1, result.end());

    return collapseWhitespace(result, true);
#else
    NOTIMPLEMENTED();
    return std::u16string();
#endif
}

//--------------------------------------------------------------------------------------------------
// static
std::u16string SysInfo::processorVendor()
{
#if defined(ARCH_CPU_X86_FAMILY)
    CpuidUtil cpuidUtil;
    cpuidUtil.get(0x00000000);

    uint32_t ebx = cpuidUtil.ebx();
    uint32_t ecx = cpuidUtil.ecx();
    uint32_t edx = cpuidUtil.edx();

    char buffer[13];
    memset(&buffer[0], 0, sizeof(buffer));

    memcpy(&buffer[0], &ebx, sizeof(ebx));
    memcpy(&buffer[4], &edx, sizeof(edx));
    memcpy(&buffer[8], &ecx, sizeof(ecx));

    std::u16string vendor = collapseWhitespace(utf16FromAscii(buffer), true);

    if (vendor == u"GenuineIntel")
        return u"Intel Corporation";
    else if (vendor == u"AuthenticAMD" || vendor == u"AMDisbetter!")
        return u"Advanced Micro Devices, Inc.";
    else if (vendor == u"CentaurHauls")
        return u"Centaur";
    else if (vendor == u"CyrixInstead")
        return u"Cyrix";
    else if (vendor == u"TransmetaCPU" || vendor == u"GenuineTMx86")
        return u"Transmeta";
    else if (vendor == u"Geode by NSC")
        return u"National Semiconductor";
    else if (vendor == u"NexGenDriven")
        return u"NexGen";
    else if (vendor == u"RiseRiseRise")
        return u"Rise";
    else if (vendor == u"SiS SiS SiS")
        return u"SiS";
    else if (vendor == u"UMC UMC UMC")
        return u"UMC";
    else if (vendor == u"VIA VIA VIA")
        return u"VIA";
    else if (vendor == u"Vortex86 SoC")
        return u"Vortex";
    else if (vendor == u"KVMKVMKVMKVM")
        return u"KVM";
    else if (vendor == u"Microsoft Hv")
        return u"Microsoft Hyper-V or Windows Virtual PC";
    else if (vendor == u"VMwareVMware")
        return u"VMware";
    else if (vendor == u"XenVMMXenVMM")
        return u"Xen HVM";
    else
        return vendor;
#else
    NOTIMPLEMENTED();
    return std::u16string();
#endif
}

} // namespace base
