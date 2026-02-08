//
// SmartCafe Project
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

#include "base/sys_info.h"

#include "base/cpuid_util.h"
#include "base/logging.h"

#include <algorithm>
#include <cstring>

namespace base {

//--------------------------------------------------------------------------------------------------
// static
QString SysInfo::processorName()
{
#if defined(Q_PROCESSOR_X86)
    CpuidUtil cpuidUtil;
    cpuidUtil.get(static_cast<int>(0x80000000));

    quint32 max_leaf = cpuidUtil.eax();
    if (max_leaf < 0x80000002)
        return QString();

    max_leaf = std::min(max_leaf, 0x80000004);

    char buffer[49];
    memset(&buffer[0], 0, sizeof(buffer));

    for (quint32 leaf = 0x80000002, offset = 0; leaf <= max_leaf; ++leaf, offset += 16)
    {
        cpuidUtil.get(static_cast<int>(leaf));

        quint32 eax = cpuidUtil.eax();
        quint32 ebx = cpuidUtil.ebx();
        quint32 ecx = cpuidUtil.ecx();
        quint32 edx = cpuidUtil.edx();

        memcpy(&buffer[offset + 0], &eax, sizeof(eax));
        memcpy(&buffer[offset + 4], &ebx, sizeof(ebx));
        memcpy(&buffer[offset + 8], &ecx, sizeof(ecx));
        memcpy(&buffer[offset + 12], &edx, sizeof(edx));
    }

    QString result = QString::fromLatin1(buffer);

    result.remove("(TM)");
    result.remove("(tm)");
    result.remove("(R)");
    result.remove("CPU");
    result.remove("Quad-Core Processor");
    result.remove("Six-Core Processor");
    result.remove("Eight-Core Processor");

    return result.trimmed();
#else
    NOTIMPLEMENTED();
    return QString();
#endif
}

//--------------------------------------------------------------------------------------------------
// static
QString SysInfo::processorVendor()
{
#if defined(Q_PROCESSOR_X86)
    CpuidUtil cpuidUtil;
    cpuidUtil.get(0x00000000);

    quint32 ebx = cpuidUtil.ebx();
    quint32 ecx = cpuidUtil.ecx();
    quint32 edx = cpuidUtil.edx();

    char buffer[13];
    memset(&buffer[0], 0, sizeof(buffer));

    memcpy(&buffer[0], &ebx, sizeof(ebx));
    memcpy(&buffer[4], &edx, sizeof(edx));
    memcpy(&buffer[8], &ecx, sizeof(ecx));

    QString vendor = QString::fromLatin1(buffer).trimmed();

    if (vendor == "GenuineIntel")
        return "Intel Corporation";
    else if (vendor == "AuthenticAMD" || vendor == "AMDisbetter!")
        return "Advanced Micro Devices, Inc.";
    else if (vendor == u"CentaurHauls")
        return "Centaur";
    else if (vendor == "CyrixInstead")
        return "Cyrix";
    else if (vendor == "TransmetaCPU" || vendor == "GenuineTMx86")
        return "Transmeta";
    else if (vendor == "Geode by NSC")
        return "National Semiconductor";
    else if (vendor == u"NexGenDriven")
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
    else if (vendor == u"VMwareVMware")
        return "VMware";
    else if (vendor == u"XenVMMXenVMM")
        return "Xen HVM";
    else
        return vendor;
#else
    NOTIMPLEMENTED();
    return QString();
#endif
}

} // namespace base
