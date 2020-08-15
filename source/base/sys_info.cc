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

#include "base/sys_info.h"

#include "base/cpuid.h"
#include "base/strings/string_util.h"

#include <algorithm>

namespace base {

// static
std::string SysInfo::processorName()
{
    CPUID cpuid;
    cpuid.get(0x80000000);

    uint32_t max_leaf = cpuid.eax();
    if (max_leaf < 0x80000002)
        return std::string();

    max_leaf = std::min(max_leaf, 0x80000004);

    char buffer[49];
    memset(&buffer[0], 0, sizeof(buffer));

    for (uint32_t leaf = 0x80000002, offset = 0; leaf <= max_leaf; ++leaf, offset += 16)
    {
        cpuid.get(leaf);

        uint32_t eax = cpuid.eax();
        uint32_t ebx = cpuid.ebx();
        uint32_t ecx = cpuid.ecx();
        uint32_t edx = cpuid.edx();

        memcpy(&buffer[offset + 0], &eax, sizeof(eax));
        memcpy(&buffer[offset + 4], &ebx, sizeof(ebx));
        memcpy(&buffer[offset + 8], &ecx, sizeof(ecx));
        memcpy(&buffer[offset + 12], &edx, sizeof(edx));
    }

    std::string result(buffer);

    removeChars(&result, "(TM)");
    removeChars(&result, "(tm)");
    removeChars(&result, "(R)");
    removeChars(&result, "CPU");
    removeChars(&result, "Quad-Core Processor");
    removeChars(&result, "Six-Core Processor");
    removeChars(&result, "Eight-Core Processor");

    std::string_view at("@");
    auto sub = find_end(result.begin(), result.end(), at.begin(), at.end());
    if (sub != result.end())
        result.erase(sub - 1, result.end());

    return collapseWhitespaceASCII(result, true);
}

// static
std::string SysInfo::processorVendor()
{
    CPUID cpuid;
    cpuid.get(0x00000000);

    uint32_t ebx = cpuid.ebx();
    uint32_t ecx = cpuid.ecx();
    uint32_t edx = cpuid.edx();

    char buffer[13];
    memset(&buffer[0], 0, sizeof(buffer));

    memcpy(&buffer[0], &ebx, sizeof(ebx));
    memcpy(&buffer[4], &edx, sizeof(edx));
    memcpy(&buffer[8], &ecx, sizeof(ecx));

    std::string vendor = collapseWhitespaceASCII(buffer, true);

    if (vendor == "GenuineIntel")
        return "Intel Corporation";
    else if (vendor == "AuthenticAMD" || vendor == "AMDisbetter!")
        return "Advanced Micro Devices, Inc.";
    else if (vendor == "CentaurHauls")
        return "Centaur";
    else if (vendor == "CyrixInstead")
        return "Cyrix";
    else if (vendor == "TransmetaCPU" || vendor == "GenuineTMx86")
        return "Transmeta";
    else if (vendor == "Geode by NSC")
        return "National Semiconductor";
    else if (vendor == "NexGenDriven")
        return "NexGen";
    else if (vendor == "RiseRiseRise")
        return "Rise";
    else if (vendor == "SiS SiS SiS")
        return "SiS";
    else if (vendor == "UMC UMC UMC")
        return "UMC";
    else if (vendor == "VIA VIA VIA")
        return "VIA";
    else if (vendor == "Vortex86 SoC")
        return "Vortex";
    else if (vendor == "KVMKVMKVMKVM")
        return "KVM";
    else if (vendor == "Microsoft Hv")
        return "Microsoft Hyper-V or Windows Virtual PC";
    else if (vendor == "VMwareVMware")
        return "VMware";
    else if (vendor == "XenVMMXenVMM")
        return "Xen HVM";
    else
        return vendor;
}

} // namespace base
