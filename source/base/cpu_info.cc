//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/cpu_info.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/cpu_info.h"
#include "base/cpuid.h"
#include "base/bitset.h"
#include "base/logging.h"

#include <memory>

namespace aspia {

void GetCPUCount(uint32_t& package_count,
                 uint32_t& physical_core_count,
                 uint32_t& logical_core_count)
{
    SYSTEM_INFO system_info;
    memset(&system_info, 0, sizeof(system_info));

    GetNativeSystemInfo(&system_info);
    logical_core_count = system_info.dwNumberOfProcessors;

    DWORD returned_length = 0;

    package_count = 0;
    physical_core_count = 0;

    if (GetLogicalProcessorInformation(nullptr, &returned_length) ||
        GetLastError() != ERROR_INSUFFICIENT_BUFFER)
    {
        DLOG(ERROR) << "Unexpected return value";
        return;
    }

    std::unique_ptr<uint8_t[]> buffer = std::make_unique<uint8_t[]>(returned_length);

    PSYSTEM_LOGICAL_PROCESSOR_INFORMATION info =
        reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION>(buffer.get());

    if (!GetLogicalProcessorInformation(info, &returned_length))
    {
        DLOG(ERROR) << "GetLogicalProcessorInformation() failed: " << GetLastSystemErrorString();
        return;
    }

    returned_length /= sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);

    for (size_t index = 0; index < returned_length; ++index)
    {
        switch (info[index].Relationship)
        {
            case RelationProcessorCore:
                ++physical_core_count;
                break;

            case RelationProcessorPackage:
                ++package_count;
                break;

            default:
                break;
        }
    }
}

void GetCPUInformation(CPUInfo& cpu_info)
{
    DCHECK_EQ(sizeof(FN_1_ECX), sizeof(uint32_t));
    DCHECK_EQ(sizeof(FN_1_EDX), sizeof(uint32_t));
    DCHECK_EQ(sizeof(FN_80000001_ECX), sizeof(uint32_t));
    DCHECK_EQ(sizeof(FN_80000001_EDX), sizeof(uint32_t));
    DCHECK_EQ(sizeof(FN_7_0_EBX), sizeof(uint32_t));
    DCHECK_EQ(sizeof(FN_7_0_ECX), sizeof(uint32_t));
    DCHECK_EQ(sizeof(FN_7_0_EDX), sizeof(uint32_t));
    DCHECK_EQ(sizeof(FN_C0000001_EDX), sizeof(uint32_t));

    memset(&cpu_info, 0, sizeof(CPUInfo));

    CPUID cpuid;

    cpuid.get(0x00000000);

    uint32_t eax = cpuid.eax();
    uint32_t ebx = cpuid.ebx();
    uint32_t ecx = cpuid.ecx();
    uint32_t edx = cpuid.edx();

    memcpy(&cpu_info.vendor[0], &ebx, sizeof(ebx));
    memcpy(&cpu_info.vendor[4], &edx, sizeof(edx));
    memcpy(&cpu_info.vendor[8], &ecx, sizeof(ecx));

    uint32_t max_leaf = cpuid.eax();

    for (uint32_t leaf = 0; leaf <= max_leaf; ++leaf)
    {
        cpuid.get(leaf);

        eax = cpuid.eax();
        ebx = cpuid.ebx();
        ecx = cpuid.ecx();
        edx = cpuid.edx();

        switch (leaf)
        {
            case 1:
            {
                cpu_info.stepping = BitSet<uint32_t>(eax).Range(0, 3);
                cpu_info.model = BitSet<uint32_t>(eax).Range(4, 7);
                cpu_info.family = BitSet<uint32_t>(eax).Range(8, 11);
                cpu_info.brand_id = BitSet<uint32_t>(ebx).Range(0, 7);

                memcpy(&cpu_info.fn_1_ecx, &ecx, sizeof(ecx));
                memcpy(&cpu_info.fn_1_edx, &edx, sizeof(edx));

                if (cpu_info.family == 0x06 || cpu_info.family == 0x0F)
                {
                    const uint32_t extended_model = BitSet<uint32_t>(eax).Range(16, 19);
                    const uint32_t extended_family = BitSet<uint32_t>(eax).Range(20, 27);
                    cpu_info.extended_model = (extended_model << 4) + cpu_info.model;
                    cpu_info.extended_family = extended_family + cpu_info.family;
                }
            }
            break;

            case 7:
            {
                memcpy(&cpu_info.fn_7_0_ebx, &ebx, sizeof(ebx));
                memcpy(&cpu_info.fn_7_0_ecx, &ecx, sizeof(ecx));
                memcpy(&cpu_info.fn_7_0_edx, &edx, sizeof(edx));
            }
            break;

            default:
                break;
        }
    }

    max_leaf = CPUID(0x80000000).eax();

    for (uint32_t leaf = 0x80000000, offset = 0; leaf <= max_leaf; ++leaf)
    {
        cpuid.get(leaf);

        eax = cpuid.eax();
        ebx = cpuid.ebx();
        ecx = cpuid.ecx();
        edx = cpuid.edx();

        switch (leaf)
        {
            case 0x80000001:
            {
                memcpy(&cpu_info.fn_81_ecx, &ecx, sizeof(ecx));
                memcpy(&cpu_info.fn_81_edx, &edx, sizeof(edx));
            }
            break;

            case 0x80000002:
            case 0x80000003:
            case 0x80000004:
            {
                memcpy(&cpu_info.brand_string[offset + 0], &eax, sizeof(eax));
                memcpy(&cpu_info.brand_string[offset + 4], &ebx, sizeof(ebx));
                memcpy(&cpu_info.brand_string[offset + 8], &ecx, sizeof(ecx));
                memcpy(&cpu_info.brand_string[offset + 12], &edx, sizeof(edx));

                offset += 16;

                cpu_info.brand_string[offset] = 0;
            }
            break;

            default:
                break;
        }
    }

    max_leaf = CPUID(0xC0000000).eax();

    for (uint32_t leaf = 0xC0000000; leaf <= max_leaf; ++leaf)
    {
        cpuid.get(leaf);

        eax = cpuid.eax();
        ebx = cpuid.ebx();
        ecx = cpuid.ecx();
        edx = cpuid.edx();

        switch (leaf)
        {
            case 0xC0000001:
            {
                memcpy(&cpu_info.fn_c1_edx, &edx, sizeof(edx));
            }
            break;

            default:
                break;
        }
    }

    GetCPUCount(cpu_info.package_count,
                cpu_info.physical_core_count,
                cpu_info.logical_core_count);
}

} // namespace aspia
