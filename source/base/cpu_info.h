//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/cpu_info.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__CPU_INFO_H
#define _ASPIA_BASE__CPU_INFO_H

#include <string>

namespace aspia {

double GetCPUSpeed();

struct CPUInfo
{
#pragma pack(push, 1)
    struct Func1Edx
    {
        uint8_t has_fpu : 1;   // Bit 0. Floating-point Unit On-Chip
        uint8_t has_vme : 1;   // Bit 1. Virtual Mode Extension
        uint8_t has_de : 1;    // Bit 2. Debug Extension
        uint8_t has_pse : 1;   // Bit 3. Page Size Extension
        uint8_t has_tsc : 1;   // Bit 4. Time Stamp Extension
        uint8_t has_msr : 1;   // Bit 5. Model Specific Registers
        uint8_t has_pae : 1;   // Bit 6. Physical Address Extension
        uint8_t has_mce : 1;   // Bit 7. Machine-Check Exception
        uint8_t has_cx8 : 1;   // Bit 8. CMPXCHG8 Instruction
        uint8_t has_apic : 1;  // Bit 9. On-cpip APIC Hardware
        uint8_t reserved1 : 1; // Bit 10. Reserved
        uint8_t has_sep : 1;   // Bit 11. Fast System Call
        uint8_t has_mtrr : 1;  // Bit 12. Memory Type Range Registers
        uint8_t has_pge : 1;   // Bit 13. Page Global Enable
        uint8_t has_mca : 1;   // Bit 14. Machine-Check Architecture
        uint8_t has_cmov : 1;  // Bit 15. Conditional Move Instruction
        uint8_t has_pat : 1;   // Bit 16. Page Attribute Table
        uint8_t has_pse36 : 1; // Bit 17. 36-bit Page Size Extension
        uint8_t has_psn : 1;   // Bit 18. Processor serial number is present and enabled
        uint8_t has_clfsh : 1; // Bit 19. CLFLUSH Instruction
        uint8_t reserved2 : 1; // Bit 20. Reserved
        uint8_t has_ds : 1;    // Bit 21. Debug Store
        uint8_t has_acpu : 1;  // Bit 22. Thermal Monitor and Software Controlled Clock Facilities
        uint8_t has_mmx : 1;   // Bit 23. MMX Technology
        uint8_t has_fxsr : 1;  // Bit 24. FXSAVE and FXSTOR Instructions
        uint8_t has_sse : 1;   // Bit 25. Streaming SIMD Extension
        uint8_t has_sse2 : 1;  // Bit 26. Streaming SIMD Extension 2
        uint8_t has_ss : 1;    // Bit 27. Self-Snoop
        uint8_t has_htt : 1;   // Bit 28. Multi-Threading
        uint8_t has_tm : 1;    // Bit 29. Thermal Monitor
        uint8_t reserved3 : 1; // Bit 30. Reserved
        uint8_t has_pbe : 1;   // Bit 31. Pending Break Enable
    };

    struct Func1Ecx
    {
        uint8_t has_sse3 : 1;         // Bit 0. Streaming SIMD Extension 3
        uint8_t has_pclmuldq : 1;     // Bit 1. PCLMULDQ Instruction
        uint8_t has_dtes64 : 1;       // Bit 2. 64-Bit Debug Store
        uint8_t has_monitor : 1;      // Bit 3. MONITOR/MWAIT Instructions
        uint8_t has_ds_cpl : 1;       // Bit 4. CPL Qualified Debug Store
        uint8_t has_vmx : 1;          // Bit 5. Virtual Machine Extensions
        uint8_t has_smx : 1;          // Bit 6. Safe Mode Extensions
        uint8_t has_est : 1;          // Bit 7. Enhanced Intel SpeedStep Technology
        uint8_t has_tm2 : 1;          // Bit 8. Thermal Monitor 2
        uint8_t has_ssse3 : 1;        // Bit 9. Supplemental Streaming SIMD Extension 3
        uint8_t has_cnxt_id : 1;      // Bit 10. L1 Context ID
        uint8_t reserved1 : 1;        // Bit 11. Reserved
        uint8_t has_fma : 1;          // Bit 12. Fused Multiply Add
        uint8_t has_cx16 : 1;         // Bit 13. CMPXCHG16B Instruction
        uint8_t has_xtpr : 1;         // Bit 14. xTPR Update Control
        uint8_t has_pdcm : 1;         // Bit 15. Perfmon and Debug Capability
        uint8_t reserved2 : 1;        // Bit 16. Reserved
        uint8_t has_pcid : 1;         // Bit 17. Process Context Identifiers
        uint8_t has_dca : 1;          // Bit 18. Direct Cache Access
        uint8_t has_sse41 : 1;        // Bit 19. Streaming SIMD Extension 4.1
        uint8_t has_sse42 : 1;        // Bit 20. Streaming SIMD Extension 4.2
        uint8_t has_x2apic : 1;       // Bit 21. Extended xAPIC Support
        uint8_t has_movbe : 1;        // Bit 22. MOVBE Instruction
        uint8_t has_popcnt : 1;       // Bit 23. POPCNT Instruction
        uint8_t has_tsc_deadline : 1; // Bit 24. Time Stamp Counter Deadline
        uint8_t has_aes : 1;          // Bit 25. AES Instruction
        uint8_t has_xsave : 1;        // Bit 26. XSAVE/XSTOR States
        uint8_t has_osxsave : 1;      // Bit 27. OS-Enabled Extended State Management
        uint8_t has_avx : 1;          // Bit 28. Advanced Vector Extension
        uint8_t has_f16c : 1;         // Bit 29. 16-bit floating-point convertsion instructions
        uint8_t has_rdrand : 1;       // Bit 30. RDRAND Instruction
        uint8_t reserved3 : 1;        // Bit 31. Not Used. Always returns 0.
    };

    struct Func81Edx
    {
        uint8_t reserved0 : 1;     // Bit 0. Reserved
        uint8_t reserved1 : 1;     // Bit 1. Reserved
        uint8_t reserved2 : 1;     // Bit 2. Reserved
        uint8_t reserved3 : 1;     // Bit 3. Reserved
        uint8_t reserved4 : 1;     // Bit 4. Reserved
        uint8_t reserved5 : 1;     // Bit 5. Reserved
        uint8_t reserved6 : 1;     // Bit 6. Reserved
        uint8_t reserved7 : 1;     // Bit 7. Reserved
        uint8_t reserved8 : 1;     // Bit 8. Reserved
        uint8_t reserved9 : 1;     // Bit 9. Reserved
        uint8_t reserved10 : 1;    // Bit 10. Reserved
        uint8_t has_syscall : 1;   // Bit 11. SYSCALL/SYSRET Instructions
        uint8_t reserved11 : 1;    // Bit 12. Reserved
        uint8_t reserved12 : 1;    // Bit 13. Reserved
        uint8_t reserved13 : 1;    // Bit 14. Reserved
        uint8_t reserved14 : 1;    // Bit 15. Reserved
        uint8_t reserved15 : 1;    // Bit 16. Reserved
        uint8_t reserved16 : 1;    // Bit 17. Reserved
        uint8_t reserved17 : 1;    // Bit 18. Reserved
        uint8_t reserved18 : 1;    // Bit 19. Reserved
        uint8_t has_xd_bit : 1;    // Bit 20. Execution Disable Bit
        uint8_t reserved19 : 1;    // Bit 21. Reserved
        uint8_t reserved20 : 1;    // Bit 22. Reserved
        uint8_t reserved21 : 1;    // Bit 23. Reserved
        uint8_t reserved22 : 1;    // Bit 24. Reserved
        uint8_t reserved23 : 1;    // Bit 25. Reserved
        uint8_t has_1gb_pages : 1; // Bit 26. 1 GB Pages
        uint8_t has_rdtscp : 1;    // Bit 27. RDTSCP and IA32_TSC_AUX Supported
        uint8_t reserved26 : 1;    // Bit 28. Reserved
        uint8_t has_intel64 : 1;   // Bit 29. Intel 64 Instruction Set Architecture
        uint8_t reserved27 : 1;    // Bit 30. Reserved
        uint8_t reserved28 : 1;    // Bit 31. Reserved
    };

    struct Func81Ecx
    {
        uint8_t has_lahf : 1; // Bit 0. LAHF/SAHF Instructions
        uint8_t reserved0 : 1; // Bit 1. Reserved
        uint8_t reserved1 : 1; // Bit 2. Reserved
        uint8_t reserved2 : 1; // Bit 3. Reserved
        uint8_t reserved3 : 1; // Bit 4. Reserved
        uint8_t reserved4 : 1; // Bit 5. Reserved
        uint8_t reserved5 : 1; // Bit 6. Reserved
        uint8_t reserved6 : 1; // Bit 7. Reserved
        uint8_t reserved7 : 1; // Bit 8. Reserved
        uint8_t reserved8 : 1; // Bit 9. Reserved
        uint8_t reserved9 : 1; // Bit 10. Reserved
        uint8_t reserved10 : 1; // Bit 11. Reserved
        uint8_t reserved11 : 1; // Bit 12. Reserved
        uint8_t reserved12 : 1; // Bit 13. Reserved
        uint8_t reserved13 : 1; // Bit 14. Reserved
        uint8_t reserved14 : 1; // Bit 15. Reserved
        uint8_t reserved15 : 1; // Bit 16. Reserved
        uint8_t reserved16 : 1; // Bit 17. Reserved
        uint8_t reserved17 : 1; // Bit 18. Reserved
        uint8_t reserved18 : 1; // Bit 19. Reserved
        uint8_t reserved19 : 1; // Bit 20. Reserved
        uint8_t reserved20 : 1; // Bit 21. Reserved
        uint8_t reserved21 : 1; // Bit 22. Reserved
        uint8_t reserved22 : 1; // Bit 23. Reserved
        uint8_t reserved23 : 1; // Bit 24. Reserved
        uint8_t reserved24 : 1; // Bit 25. Reserved
        uint8_t reserved25 : 1; // Bit 26. Reserved
        uint8_t reserved26 : 1; // Bit 27. Reserved
        uint8_t reserved27 : 1; // Bit 28. Reserved
        uint8_t reserved28 : 1; // Bit 29. Reserved
        uint8_t reserved29 : 1; // Bit 30. Reserved
        uint8_t reserved30 : 1; // Bit 31. Reserved
    };
#pragma pack(pop)

    char brand_string[49];
    char manufacturer[13];
    uint32_t family;
    uint32_t model;
    uint32_t stepping;
    uint32_t extended_family;
    uint32_t extended_model;
    uint32_t brand_id;
    uint32_t package_count;
    uint32_t physical_core_count;
    uint32_t logical_core_count;
    Func1Edx func1_edx;
    Func1Ecx func1_ecx;
    Func81Edx func81_edx;
    Func81Ecx func81_ecx;
};

void GetCPUInformation(CPUInfo& cpu_info);

} // namespace aspia

#endif // _ASPIA_BASE__CPU_INFO_H
