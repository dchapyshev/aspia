//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/cpu_info.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__CPU_INFO_H
#define _ASPIA_BASE__CPU_INFO_H

#include <cstdint>

namespace aspia {

#pragma pack(push, 1)
struct FN_1_EDX
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
    uint8_t has_ia64 : 1;  // Bit 30. IA64 Processor Emulating x86
    uint8_t has_pbe : 1;   // Bit 31. Pending Break Enable
};

struct FN_1_ECX
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
    uint8_t has_sdbg : 1;         // Bit 11. Silicon Debug Interface
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
    uint8_t has_hypervisor : 1;   // Bit 31. Hypervisor
};

struct FN_80000001_EDX
{
    uint8_t reserved0 : 1;      // Bit 0. Reserved
    uint8_t reserved1 : 1;      // Bit 1. Reserved
    uint8_t reserved2 : 1;      // Bit 2. Reserved
    uint8_t reserved3 : 1;      // Bit 3. Reserved
    uint8_t reserved4 : 1;      // Bit 4. Reserved
    uint8_t reserved5 : 1;      // Bit 5. Reserved
    uint8_t reserved6 : 1;      // Bit 6. Reserved
    uint8_t reserved7 : 1;      // Bit 7. Reserved
    uint8_t reserved8 : 1;      // Bit 8. Reserved
    uint8_t reserved9 : 1;      // Bit 9. Reserved
    uint8_t reserved10 : 1;     // Bit 10. Reserved
    uint8_t has_syscall : 1;    // Bit 11. SYSCALL/SYSRET Instructions
    uint8_t reserved11 : 1;     // Bit 12. Reserved
    uint8_t reserved12 : 1;     // Bit 13. Reserved
    uint8_t reserved13 : 1;     // Bit 14. Reserved
    uint8_t reserved14 : 1;     // Bit 15. Reserved
    uint8_t reserved15 : 1;     // Bit 16. Reserved
    uint8_t reserved16 : 1;     // Bit 17. Reserved
    uint8_t reserved17 : 1;     // Bit 18. Reserved
    uint8_t reserved18 : 1;     // Bit 19. Reserved
    uint8_t has_xd_bit : 1;     // Bit 20. Execution Disable Bit
    uint8_t reserved19 : 1;     // Bit 21. Reserved
    uint8_t has_mmxext : 1;     // Bit 22. AMD Extended MMX
    uint8_t reserved20 : 1;     // Bit 23. Reserved
    uint8_t reserved21 : 1;     // Bit 24. Reserved
    uint8_t reserved22 : 1;     // Bit 25. Reserved
    uint8_t has_1gb_pages : 1;  // Bit 26. 1 GB Pages
    uint8_t has_rdtscp : 1;     // Bit 27. RDTSCP Instruction
    uint8_t reserved23 : 1;     // Bit 28. Reserved
    uint8_t has_intel64 : 1;    // Bit 29. Intel 64 Instruction Set Architecture
    uint8_t has_3dnowext : 1;   // Bit 30. AMD Extended 3DNow!
    uint8_t has_3dnow : 1;      // Bit 31. AMD 3DNow!
};

struct FN_80000001_ECX
{
    uint8_t has_lahf : 1;           // Bit 0. LAHF/SAHF Instructions
    uint8_t reserved0 : 1;          // Bit 1. Reserved
    uint8_t has_svm : 1;            // Bit 2. Secure Virtual Machine (SVM, Pacifica)
    uint8_t reserved2 : 1;          // Bit 3. Reserved
    uint8_t reserved3 : 1;          // Bit 4. Reserved
    uint8_t has_lzcnt : 1;          // Bit 5. Reserved
    uint8_t has_sse4a : 1;          // Bit 6. AMD SSE4A
    uint8_t has_misalignsse : 1;    // Bit 7. AMD MisAligned SSE
    uint8_t has_3dnow_prefetch : 1; // Bit 8. AMD 3DNowPrefetch
    uint8_t reserved5 : 1;          // Bit 9. Reserved
    uint8_t reserved6 : 1;          // Bit 10. Reserved
    uint8_t has_xop : 1;            // Bit 11. AMD XOP
    uint8_t reserved7 : 1;          // Bit 12. Reserved
    uint8_t has_wdt : 1;            // Bit 13. Watchdog Timer
    uint8_t reserved9 : 1;          // Bit 14. Reserved
    uint8_t reserved10 : 1;         // Bit 15. Reserved
    uint8_t has_fma4 : 1;           // Bit 16. AMD FMA4
    uint8_t reserved11 : 1;         // Bit 17. Reserved
    uint8_t reserved12 : 1;         // Bit 18. Reserved
    uint8_t reserved13 : 1;         // Bit 19. Reserved
    uint8_t reserved14 : 1;         // Bit 20. Reserved
    uint8_t reserved15 : 1;         // Bit 21. Reserved
    uint8_t reserved16 : 1;         // Bit 22. Reserved
    uint8_t reserved17 : 1;         // Bit 23. Reserved
    uint8_t reserved18 : 1;         // Bit 24. Reserved
    uint8_t reserved19 : 1;         // Bit 25. Reserved
    uint8_t reserved20 : 1;         // Bit 26. Reserved
    uint8_t reserved21 : 1;         // Bit 27. Reserved
    uint8_t reserved22 : 1;         // Bit 28. Reserved
    uint8_t reserved23 : 1;         // Bit 29. Reserved
    uint8_t reserved24 : 1;         // Bit 30. Reserved
    uint8_t reserved25 : 1;         // Bit 31. Reserved
};

struct FN_7_0_EBX
{
    uint8_t has_fsgsbase : 1;   // Bit 0. RDFSBASE / RDGSBASE / WRFSBASE / WRGSBASE Instruction
    uint8_t reserved0 : 1;      // Bit 1. Reserved
    uint8_t has_sgx : 1;        // Bit 2. Software Guard Extensions
    uint8_t has_bmi1 : 1;       // Bit 3. Bit Manipulation Instruction Set 1
    uint8_t has_hle : 1;        // Bit 4. Transactional Synchronization Extensions
    uint8_t has_avx2 : 1;       // Bit 5. Advanced Vector Extensions 2
    uint8_t reserved1 : 1;      // Bit 6. Reserved
    uint8_t has_smep : 1;       // Bit 7. Supervisor-Mode Execution Prevention
    uint8_t has_bmi2 : 1;       // Bit 8. Bit Manipulation Instruction Set 2
    uint8_t has_erms : 1;       // Bit 9. Enhanced REP MOVSB/STOSB
    uint8_t has_invpcid : 1;    // Bit 10. INVPCID Instruction
    uint8_t has_rtm : 1;        // Bit 11. Transactional Synchronization Extensions
    uint8_t has_pqm : 1;        // Bit 12. Platform Quality of Service Monitoring
    uint8_t reserved2 : 1;      // Bit 13. Reserved
    uint8_t has_mpx : 1;        // Bit 14. Memory Protection Extensions
    uint8_t has_pqe : 1;        // Bit 15. Platform Quality of Service Enforcement
    uint8_t has_avx512f : 1;    // Bit 16. AVX-512 Foundation
    uint8_t has_avx512dq : 1;   // Bit 17. AVX-512 Doubleword and Quadword Instructions
    uint8_t has_rdseed : 1;     // Bit 18. RDSEED Instruction
    uint8_t has_adx : 1;        // Bit 19. Multi-Precision Add-Carry Instruction Extensions
    uint8_t has_smap : 1;       // Bit 20. Supervisor Mode Access Prevention
    uint8_t has_avx512ifma : 1; // Bit 21. AVX-512 Integer Fused Multiply-Add Instructions
    uint8_t has_pcommit : 1;    // Bit 22. PCOMMIT Instruction
    uint8_t has_clflushopt : 1; // Bit 23. CLFLUSHOPT Instruction
    uint8_t has_clwb : 1;       // Bit 24. CLWB instruction
    uint8_t has_intel_pt : 1;   // Bit 25. Intel Processor Trace
    uint8_t has_avx512pf : 1;   // Bit 26. AVX-512 Prefetch Instructions
    uint8_t has_avx512er : 1;   // Bit 27. AVX-512 Exponential and Reciprocal Instructions
    uint8_t has_avx512cd : 1;   // Bit 28. AVX-512 Conflict Detection Instructions
    uint8_t has_sha : 1;        // Bit 29. Intel SHA Extensions
    uint8_t has_avx512bw : 1;   // Bit 30. AVX-512 Byte and Word Instructions
    uint8_t has_avx512vl : 1;   // Bit 31. AVX-512 Vector Length Extensions
};

struct FN_7_0_ECX
{
    uint8_t has_prefetchwt1 : 1;     // Bit 0. PREFETCHWT1 Instruction
    uint8_t has_avx512vbmi : 1;      // Bit 1. AVX-512 Vector Bit Manipulation Instructions
    uint8_t has_umip : 1;            // Bit 2. User-mode Instruction Prevention
    uint8_t has_pku : 1;             // Bit 3. Memory Protection Keys for User-mode pages
    uint8_t has_ospke : 1;           // Bit 4. PKU enabled by OS
    uint8_t reserved0 : 1;           // Bit 5. Reserved
    uint8_t has_avx512vbmi2 : 1;     // Bit 6. AVX-512 Vector Bit Manipulation Instructions 2
    uint8_t reserved1 : 1;           // Bit 7. Reserved
    uint8_t has_gfni : 1;            // Bit 8. Galois Field Instructions
    uint8_t has_vaes : 1;            // Bit 9. AES Instruction Set (VEX-256/EVEX)
    uint8_t has_vpclmulqdq : 1;      // Bit 10. CLMUL Instruction Set (VEX-256/EVEX)
    uint8_t has_avx512vnni : 1;      // Bit 11. AVX-512 Vector Neural Network Instructions
    uint8_t has_avx512bitalg : 1;    // Bit 12. AVX-512 BITALG Instructions
    uint8_t reserved2 : 1;           // Bit 13. Reserved
    uint8_t has_avx512vpopcntdq : 1; // Bit 14. AVX-512 Vector Population Count D/Q
    uint8_t reserved3 : 1;           // Bit 15. Reserved
    uint8_t reserved4 : 1;           // Bit 16. Reserved
    uint8_t mawau : 5; // Bit 17-21. The value of userspace MPX Address-Width Adjust used
                       // by the BNDLDX and BNDSTX Intel MPX instructions in 64 - bit mode
    uint8_t has_rdpid : 1;           // Bit 22. Read Processor ID
    uint8_t reserved5 : 1;           // Bit 23. Reserved
    uint8_t reserved6 : 1;           // Bit 24. Reserved
    uint8_t reserved7 : 1;           // Bit 25. Reserved
    uint8_t reserved8 : 1;           // Bit 26. Reserved
    uint8_t reserved9 : 1;           // Bit 27. Reserved
    uint8_t reserved10 : 1;          // Bit 28. Reserved
    uint8_t reserved11 : 1;          // Bit 29. Reserved
    uint8_t has_sgx_lc : 1;          // Bit 30. SGX Launch Configuration
    uint8_t reserved12 : 1;          // Bit 31. Reserved
};

struct FN_7_0_EDX
{
    uint8_t reserved0 : 1;         // Bit 0. Reserved
    uint8_t reserved1 : 1;         // Bit 1. Reserved
    uint8_t has_avx512_4vnniw : 1; // Bit 2. AVX-512 4-register Neural Network Instructions
    uint8_t has_avx512_4fmaps : 1; // Bit 3. AVX-512 4-register Multiply Accumulation Single precision
    uint8_t reserved2 : 1;         // Bit 4. Reserved
    uint8_t reserved3 : 1;         // Bit 5. Reserved
    uint8_t reserved4 : 1;         // Bit 6. Reserved
    uint8_t reserved5 : 1;         // Bit 7. Reserved
    uint8_t reserved6 : 1;         // Bit 8. Reserved
    uint8_t reserved7 : 1;         // Bit 9. Reserved
    uint8_t reserved8 : 1;         // Bit 10. Reserved
    uint8_t reserved9 : 1;         // Bit 11. Reserved
    uint8_t reserved10 : 1;        // Bit 12. Reserved
    uint8_t reserved11 : 1;        // Bit 13. Reserved
    uint8_t reserved12 : 1;        // Bit 14. Reserved
    uint8_t reserved13 : 1;        // Bit 15. Reserved
    uint8_t reserved14 : 1;        // Bit 16. Reserved
    uint8_t reserved15 : 1;        // Bit 17. Reserved
    uint8_t reserved16 : 1;        // Bit 18. Reserved
    uint8_t reserved17 : 1;        // Bit 19. Reserved
    uint8_t reserved18 : 1;        // Bit 20. Reserved
    uint8_t reserved19 : 1;        // Bit 21. Reserved
    uint8_t reserved20 : 1;        // Bit 22. Reserved
    uint8_t reserved21 : 1;        // Bit 23. Reserved
    uint8_t reserved22 : 1;        // Bit 24. Reserved
    uint8_t reserved23 : 1;        // Bit 25. Reserved
    uint8_t reserved24 : 1;        // Bit 26. Reserved
    uint8_t reserved25 : 1;        // Bit 27. Reserved
    uint8_t reserved26 : 1;        // Bit 28. Reserved
    uint8_t reserved27 : 1;        // Bit 29. Reserved
    uint8_t reserved28 : 1;        // Bit 30. Reserved
    uint8_t reserved29 : 1;        // Bit 31. Reserved
};

struct FN_C0000001_EDX
{
    uint8_t has_ais : 1;          // Bit 0. Alternate Instruction Set
    uint8_t has_ais_e : 1;        // Bit 1. Alternate Instruction Set enabled
    uint8_t has_rng : 1;          // Bit 2. Random Number Generator
    uint8_t has_rng_e : 1;        // Bit 3. Random Number Generator enabled
    uint8_t has_lh : 1;           // Bit 4. LongHaul MSR 0000_110Ah
    uint8_t has_femms : 1;        // Bit 5. VIA FEMMS Instruction
    uint8_t has_ace : 1;          // Bit 6. Advanced Cryptography Engine
    uint8_t has_ace_e : 1;        // Bit 7. Advanced Cryptography Engine enabled
    uint8_t has_ace2 : 1;         // Bit 8. Advanced Cryptography Engine 2
    uint8_t has_ace2_e : 1;       // Bit 9. Advanced Cryptography Engine 2 enabled
    uint8_t has_phe : 1;          // Bit 10. PadLock Hash Engine (PHE)
    uint8_t has_phe_e : 1;        // Bit 11. PadLock Hash Engine (PHE) enabled
    uint8_t has_pmm : 1;          // Bit 12. PadLock Montgomery Multiplier
    uint8_t has_pmm_e : 1;        // Bit 13. PadLock Montgomery Multiplier enabled
    uint8_t reserved4 : 1;        // Bit 14. Reserved
    uint8_t reserved5 : 1;        // Bit 15. Reserved
    uint8_t has_parallax : 1;     // Bit 16. Parallax
    uint8_t has_parallax_e : 1;   // Bit 17. Parallax enabled
    uint8_t has_overstress : 1;   // Bit 18. Overstress
    uint8_t has_overstress_e : 1; // Bit 19. Overstress enabled
    uint8_t has_tm3 : 1;          // Bit 20. Temperature Monitoring 3
    uint8_t has_tm3_e : 1;        // Bit 21. Temperature Monitoring 3 enabled
    uint8_t has_rng2 : 1;         // Bit 22. Random Number Generator 2
    uint8_t has_rng2_e : 1;       // Bit 23. Random Number Generator 2 enabled
    uint8_t reserved14 : 1;       // Bit 24. Reserved
    uint8_t has_phe2 : 1;         // Bit 25. PadLock Hash Engine 2 (PHE2)
    uint8_t has_phe2_e : 1;       // Bit 26. PadLock Hash Engine 2 (PHE2) enabled
    uint8_t reserved17 : 1;       // Bit 27. Reserved
    uint8_t reserved18 : 1;       // Bit 28. Reserved
    uint8_t reserved19 : 1;       // Bit 29. Reserved
    uint8_t reserved20 : 1;       // Bit 30. Reserved
    uint8_t reserved21 : 1;       // Bit 31. Reserved
};
#pragma pack(pop)

struct CPUInfo
{
    char brand_string[49];
    char vendor[13];
    uint32_t family;
    uint32_t model;
    uint32_t stepping;
    uint32_t extended_family;
    uint32_t extended_model;
    uint32_t brand_id;
    uint32_t package_count;
    uint32_t physical_core_count;
    uint32_t logical_core_count;
    FN_1_ECX fn_1_ecx;
    FN_1_EDX fn_1_edx;
    FN_80000001_ECX fn_81_ecx;
    FN_80000001_EDX fn_81_edx;
    FN_7_0_EBX fn_7_0_ebx;
    FN_7_0_ECX fn_7_0_ecx;
    FN_7_0_EDX fn_7_0_edx;
    FN_C0000001_EDX fn_c1_edx;
};

void GetCPUInformation(CPUInfo& cpu_info);

} // namespace aspia

#endif // _ASPIA_BASE__CPU_INFO_H
