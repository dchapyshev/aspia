//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "host/cpu_info.h"

#include <QByteArray>
#include <QHash>
#include <QString>
#include <QStringList>

#include <span>
#include <utility>

#if defined(Q_PROCESSOR_ARM_64)
#if defined(Q_OS_WINDOWS)
#include <qt_windows.h>
#elif defined(Q_OS_LINUX)
#include <sys/auxv.h>
#elif defined(Q_OS_MACOS)
#include <sys/sysctl.h>
#endif
#endif // defined(Q_PROCESSOR_ARM_64)

#include "base/cpuid_util.h"
#include "base/sys_info.h"
#include "proto/system_info.h"

namespace {

// The groups a feature of the processor can be shown under, one repeated field of the report
// each. Both architectures fill the same groups, and a feature is listed where it is looked for
// and not where its bit or field happened to be placed.
enum CpuFeatureGroup
{
    GROUP_INSTRUCTION_SET = 0,
    GROUP_SECURITY,
    GROUP_POWER,
    GROUP_VIRTUALIZATION,
    GROUP_OTHER
};

//--------------------------------------------------------------------------------------------------
proto::system_info::Processor::Feature* addFeature(
    proto::system_info::Processor* processor, CpuFeatureGroup group)
{
    switch (group)
    {
        case GROUP_INSTRUCTION_SET: return processor->add_instruction_set();
        case GROUP_SECURITY:        return processor->add_security();
        case GROUP_POWER:           return processor->add_power_management();
        case GROUP_VIRTUALIZATION:  return processor->add_virtualization();
        default:                    return processor->add_other();
    }
}

#if defined(Q_PROCESSOR_X86)

enum CpuidRegister
{
    REG_EAX,
    REG_EBX,
    REG_ECX,
    REG_EDX
};

struct CpuidFeatureBit
{
    quint32 leaf;
    quint32 subleaf;
    CpuidRegister reg;
    int bit;
    const char* name;
};

//--------------------------------------------------------------------------------------------------
// Instruction set.
//--------------------------------------------------------------------------------------------------
constexpr CpuidFeatureBit kCpuidInstructionSet[] =
{
    { 0x80000001, 0, REG_EDX, 29, "64-bit x86 Extension (AMD64, Intel64)" },
    { 0x00000001, 0, REG_ECX, 25, "AES Instruction (AES)" },
    { 0x00000007, 0, REG_ECX,  9, "AES Instruction Set (VEX-256/EVEX)" },
    { 0x80000001, 0, REG_EDX, 31, "AMD 3DNow!" },
    { 0x80000001, 0, REG_ECX,  8, "AMD 3DNowPrefetch" },
    { 0x80000001, 0, REG_EDX, 30, "AMD Extended 3DNow!" },
    { 0x80000001, 0, REG_EDX, 22, "AMD Extended MMX" },
    { 0x80000001, 0, REG_ECX, 16, "AMD FMA4" },
    { 0x80000001, 0, REG_ECX,  7, "AMD MisAligned SSE" },
    { 0x80000001, 0, REG_ECX,  6, "AMD SSE4A" },
    { 0x80000001, 0, REG_ECX, 11, "AMD XOP" },
    { 0x00000001, 0, REG_ECX, 28, "Advanced Vector Extension (AVX)" },
    { 0x00000007, 0, REG_EBX,  5, "Advanced Vector Extensions 2 (AVX2)" },
    { 0x00000007, 1, REG_EDX, 21, "Advanced Performance Extensions (APX_F)" },
    { 0x00000007, 0, REG_EDX,  3, "AVX-512 4-register Multiply Accumulation Single Precision (AVX5124FMAPS)" },
    { 0x00000007, 0, REG_EDX,  2, "AVX-512 4-register Neural Network Instructions (AVX5124VNNIW)" },
    { 0x00000007, 0, REG_ECX, 12, "AVX-512 BITALG Instructions (AVX512BITALG)" },
    { 0x00000007, 0, REG_EBX, 30, "AVX-512 Byte and Word Instructions (AVX512BW)" },
    { 0x00000007, 0, REG_EBX, 28, "AVX-512 Conflict Detection Instructions (AVX512CD)" },
    { 0x00000007, 0, REG_EBX, 17, "AVX-512 Doubleword and Quadword Instructions (AVX512DQ)" },
    { 0x00000007, 0, REG_EBX, 27, "AVX-512 Exponential and Reciprocal Instructions (AVX512ER)" },
    { 0x00000007, 0, REG_EBX, 16, "AVX-512 Foundation (AVX512F)" },
    { 0x00000007, 0, REG_EDX, 23, "AVX-512 Half-Precision (AVX512_FP16)" },
    { 0x00000007, 0, REG_EBX, 21, "AVX-512 Integer Fused Multiply-Add Instructions (AVX512IFMA)" },
    { 0x00000007, 0, REG_EBX, 26, "AVX-512 Prefetch Instructions (AVX512PF)" },
    { 0x00000007, 0, REG_ECX,  1, "AVX-512 Vector Bit Manipulation Instructions (AVX512VBMI)" },
    { 0x00000007, 0, REG_ECX,  6, "AVX-512 Vector Bit Manipulation Instructions 2 (AVX512VBMI2)" },
    { 0x00000007, 0, REG_EBX, 31, "AVX-512 Vector Length Extensions (AVX512VL)" },
    { 0x00000007, 0, REG_ECX, 11, "AVX-512 Vector Neural Network Instructions (AVX512VNNI)" },
    { 0x00000007, 0, REG_EDX,  8, "AVX-512 Vector Pair Intersection (AVX512_VP2INTERSECT)" },
    { 0x00000007, 0, REG_ECX, 14, "AVX-512 Vector Population Count D/Q (AVX512VPOPCNTDQ)" },
    { 0x00000007, 1, REG_EAX,  5, "AVX-512 bfloat16 (AVX512_BF16)" },
    { 0x00000007, 1, REG_EAX, 23, "AVX Integer Fused Multiply-Add (AVX-IFMA)" },
    { 0x00000007, 1, REG_EAX,  4, "AVX Vector Neural Network Instructions (AVX-VNNI)" },
    { 0x00000007, 1, REG_EDX,  5, "AVX-NE-CONVERT" },
    { 0x00000007, 1, REG_EDX,  4, "AVX-VNNI-INT8" },
    { 0x00000007, 1, REG_EDX, 10, "AVX-VNNI-INT16" },
    { 0x00000007, 1, REG_EDX, 19, "AVX10" },
    { 0x00000007, 0, REG_EDX, 24, "Advanced Matrix Extensions (AMX-TILE)" },
    { 0x00000007, 0, REG_EDX, 25, "AMX Computation on 8-bit Integers (AMX-INT8)" },
    { 0x00000007, 0, REG_EDX, 22, "AMX Computation on bfloat16 (AMX-BF16)" },
    { 0x00000007, 1, REG_EDX,  8, "AMX Computation on Complex Numbers (AMX-COMPLEX)" },
    { 0x00000007, 1, REG_EAX, 21, "AMX Computation on Half-Precision (AMX-FP16)" },
    { 0x00000007, 0, REG_EBX,  3, "Bit Manipulation Instruction Set 1 (BMI1)" },
    { 0x00000007, 0, REG_EBX,  8, "Bit Manipulation Instruction Set 2 (BMI2)" },
    { 0x00000007, 0, REG_ECX, 25, "CLDEMOTE Instruction" },
    { 0x00000001, 0, REG_EDX, 19, "CLFLUSH Instruction" },
    { 0x00000007, 0, REG_EBX, 23, "CLFLUSHOPT Instruction" },
    { 0x00000007, 0, REG_ECX, 10, "CLMUL Instruction Set (VEX-256/EVEX)" },
    { 0x00000007, 0, REG_EBX, 24, "CLWB Instruction" },
    { 0x80000008, 0, REG_EBX,  0, "CLZERO Instruction" },
    { 0x00000007, 1, REG_EAX,  7, "CMPCCXADD Instruction" },
    { 0x00000001, 0, REG_EDX,  8, "CMPXCHG8B Instruction" },
    { 0x00000001, 0, REG_ECX, 13, "CMPXCHG16B Instruction" },
    { 0x00000001, 0, REG_EDX, 15, "Conditional Move Instruction (CMOV)" },
    { 0x00000007, 0, REG_EBX,  9, "Enhanced REP MOVSB/STOSB" },
    { 0x00000007, 0, REG_ECX, 29, "Enqueue Stores (ENQCMD)" },
    // The short-string features and PREFETCHI are enumerated by Intel in leaf 7 subleaf 1 and by
    // AMD in leaf 80000021h. Rows under the same name are one feature with a source per vendor.
    { 0x00000007, 1, REG_EAX, 12, "Fast Short REP CMPSB/SCASB (FSRC)" },
    { 0x80000021, 0, REG_EAX, 11, "Fast Short REP CMPSB/SCASB (FSRC)" },
    { 0x00000007, 0, REG_EDX,  4, "Fast Short REP MOVSB (FSRM)" },
    { 0x00000007, 1, REG_EAX, 11, "Fast Short REP STOSB (FSRS)" },
    { 0x80000021, 0, REG_EAX, 10, "Fast Short REP STOSB (FSRS)" },
    { 0x00000007, 1, REG_EAX, 10, "Fast Zero-Length REP MOVSB (FZRM)" },
    { 0x00000001, 0, REG_ECX, 29, "Float-16-bit Conversion Instructions (F16C)" },
    { 0x00000001, 0, REG_EDX,  0, "Floating-point Unit On-Chip (FPU)" },
    { 0x00000001, 0, REG_ECX, 12, "Fused Multiply Add (FMA)" },
    { 0x00000001, 0, REG_EDX, 24, "FXSAVE / FXSTOR Instruction" },
    { 0x00000007, 0, REG_ECX,  8, "Galois Field Instructions (GFNI)" },
    { 0x00000007, 1, REG_EAX, 22, "HRESET Instruction" },
    { 0x00000001, 0, REG_EDX, 30, "IA-64" },
    { 0x80000008, 0, REG_EBX,  3, "INVLPGB / TLBSYNC Instruction" },
    { 0x00000007, 0, REG_EBX, 10, "INVPCID Instruction" },
    { 0x80000001, 0, REG_ECX,  0, "LAHF / SAHF Instruction" },
    { 0x00000007, 1, REG_EAX, 18, "LKGS Instruction" },
    { 0x80000001, 0, REG_ECX,  5, "LZCNT Instruction" },
    { 0x80000008, 0, REG_EBX,  8, "MCOMMIT Instruction" },
    { 0x00000001, 0, REG_EDX, 23, "MMX Technology (MMX)" },
    { 0x00000001, 0, REG_ECX,  3, "MONITOR / MWAIT Instruction" },
    { 0x80000001, 0, REG_ECX, 29, "MONITORX / MWAITX Instruction" },
    { 0x00000001, 0, REG_ECX, 22, "MOVBE Instruction" },
    { 0x00000007, 0, REG_ECX, 28, "MOVDIR64B Instruction" },
    { 0x00000007, 0, REG_ECX, 27, "MOVDIRI Instruction" },
    { 0x00000007, 1, REG_EAX, 27, "MSRLIST Instruction" },
    { 0x00000007, 0, REG_EBX, 19, "Multi-Precision Add-Carry Instruction Extensions (ADX)" },
    { 0x00000001, 0, REG_ECX,  1, "PCLMULDQ Instruction" },
    { 0x00000007, 0, REG_EBX, 22, "PCOMMIT Instruction" },
    { 0x00000001, 0, REG_ECX, 23, "POPCNT Instruction" },
    { 0x00000007, 1, REG_EDX, 14, "PREFETCHI Instruction" },
    { 0x80000021, 0, REG_EAX, 20, "PREFETCHI Instruction" },
    { 0x00000007, 0, REG_ECX,  0, "PREFETCHWT1 Instruction" },
    { 0x00000014, 0, REG_EBX,  4, "PTWRITE Instruction" },
    { 0x00000007, 1, REG_EAX,  3, "RAO-INT Instructions" },
    { 0x00000007, 0, REG_EBX,  0, "RDFSBASE / RDGSBASE / WRFSBASE / WRGSBASE Instruction" },
    { 0x00000007, 0, REG_ECX, 22, "RDPID Instruction" },
    { 0x80000008, 0, REG_EBX,  4, "RDPRU Instruction" },
    { 0x00000001, 0, REG_ECX, 30, "RDRAND Instruction" },
    { 0x00000007, 0, REG_EBX, 18, "RDSEED Instruction" },
    { 0x80000001, 0, REG_EDX, 27, "RDTSCP Instruction" },
    { 0x00000007, 0, REG_EDX, 14, "SERIALIZE Instruction" },
    { 0x00000007, 0, REG_EBX, 29, "SHA Extensions (SHA)" },
    { 0x00000007, 1, REG_EAX,  0, "SHA-512 Extensions (SHA512)" },
    { 0x80000001, 0, REG_ECX, 12, "SKINIT / STGI Instruction" },
    { 0x00000007, 1, REG_EAX,  1, "SM3 Hash Instructions (SM3)" },
    { 0x00000007, 1, REG_EAX,  2, "SM4 Cipher Instructions (SM4)" },
    { 0x00000001, 0, REG_EDX, 25, "Streaming SIMD Extension (SSE)" },
    { 0x00000001, 0, REG_EDX, 26, "Streaming SIMD Extension 2 (SSE2)" },
    { 0x00000001, 0, REG_ECX,  0, "Streaming SIMD Extension 3 (SSE3)" },
    { 0x00000001, 0, REG_ECX, 19, "Streaming SIMD Extension 4.1 (SSE4.1)" },
    { 0x00000001, 0, REG_ECX, 20, "Streaming SIMD Extension 4.2 (SSE4.2)" },
    { 0x00000001, 0, REG_ECX,  9, "Supplemental Streaming SIMD Extension 3 (SSSE3)" },
    { 0x80000001, 0, REG_EDX, 11, "SYSCALL / SYSRET Instruction" },
    { 0x00000007, 0, REG_ECX,  5, "TPAUSE / UMONITOR / UMWAIT Instruction (WAITPKG)" },
    { 0x80000001, 0, REG_ECX, 21, "Trailing Bit Manipulation Instructions (TBM)" },
    { 0xC0000001, 0, REG_EDX,  0, "VIA Alternate Instruction Set" },
    { 0x80000008, 0, REG_EBX,  9, "WBNOINVD Instruction" },
    { 0x00000007, 1, REG_EAX, 19, "WRMSRNS Instruction" }
};

//--------------------------------------------------------------------------------------------------
// Security features.
//--------------------------------------------------------------------------------------------------
constexpr CpuidFeatureBit kCpuidSecurity[] =
{
    { 0xC0000001, 0, REG_EDX,  6, "Advanced Cryptography Engine (ACE)" },
    { 0xC0000001, 0, REG_EDX,  8, "Advanced Cryptography Engine 2 (ACE2)" },
    { 0x80000021, 0, REG_EAX,  8, "Automatic IBRS" },
    { 0x00000007, 0, REG_EDX, 20, "Control-flow Enforcement Indirect Branch Tracking (CET_IBT)" },
    { 0x00000007, 0, REG_ECX,  7, "Control-flow Enforcement Shadow Stack (CET_SS)" },
    { 0x00000007, 1, REG_EDX, 18, "Control-flow Enforcement Supervisor Shadow Stack (CET_SSS)" },
    { 0x80000021, 0, REG_EAX, 18, "Enhanced Predictive Store Forwarding (EPSF)" },
    { 0x80000001, 0, REG_EDX, 20, "Execution Disable Bit (NX, XD)" },
    // The speculation controls are enumerated by each vendor in a leaf of its own: Intel in leaf 7
    // (IBRS and IBPB share a bit there), AMD in leaf 80000008h with a bit per control. Rows under
    // the same name are one feature with a source per vendor.
    { 0x80000008, 0, REG_EBX, 12, "Indirect Branch Prediction Barrier (IBPB)" },
    { 0x00000007, 0, REG_EDX, 26, "Indirect Branch Prediction Barrier (IBPB)" },
    { 0x00000007, 0, REG_EDX, 26, "Indirect Branch Restricted Speculation (IBRS)" },
    { 0x80000008, 0, REG_EBX, 14, "Indirect Branch Restricted Speculation (IBRS)" },
    { 0x00000007, 0, REG_ECX, 23, "Key Locker (KL)" },
    { 0x00000007, 0, REG_EDX, 28, "L1 Data Cache Flush (L1D_FLUSH)" },
    { 0x00000007, 1, REG_EAX, 26, "Linear Address Masking (LAM)" },
    { 0x00000007, 1, REG_EAX,  6, "Linear Address Space Separation (LASS)" },
    { 0x00000007, 0, REG_EBX, 14, "Memory Protection Extensions (MPX)" },
    { 0x00000007, 0, REG_ECX, 31, "Memory Protection Keys for Supervisor-mode Pages (PKS)" },
    { 0x00000007, 0, REG_ECX,  3, "Memory Protection Keys for User-mode Pages (PKU)" },
    { 0xC0000001, 0, REG_EDX, 10, "PadLock Hash Engine (PHE)" },
    { 0xC0000001, 0, REG_EDX, 12, "PadLock Montgomery Multiplier (PMM)" },
    { 0xC0000001, 0, REG_EDX,  2, "PadLock Random Number Generator (RNG)" },
    { 0x00000007, 0, REG_ECX,  4, "PKU Enabled by OS (OSPKE)" },
    { 0x80000008, 0, REG_EBX, 28, "Predictive Store Forwarding Disable (PSFD)" },
    { 0x00000001, 0, REG_EDX, 18, "Processor Serial Number (PSN)" },
    { 0x00000001, 0, REG_ECX,  6, "Safe Mode Extensions (SMX)" },
    { 0x8000001F, 0, REG_EAX,  0, "Secure Memory Encryption (SME)" },
    { 0x00000007, 0, REG_EDX,  1, "SGX Attestation Services (SGX-KEYS)" },
    { 0x00000007, 0, REG_ECX, 30, "SGX Launch Configuration" },
    { 0x00000007, 0, REG_EDX, 27, "Single Thread Indirect Branch Predictors (STIBP)" },
    { 0x80000008, 0, REG_EBX, 15, "Single Thread Indirect Branch Predictors (STIBP)" },
    { 0x00000007, 0, REG_EBX,  2, "Software Guard Extensions (SGX)" },
    { 0x00000007, 0, REG_EDX,  9, "Special Register Buffer Data Sampling Control (SRBDS_CTRL)" },
    { 0x00000007, 0, REG_EDX, 31, "Speculative Store Bypass Disable (SSBD)" },
    { 0x80000008, 0, REG_EBX, 24, "Speculative Store Bypass Disable (SSBD)" },
    { 0x00000007, 0, REG_EBX, 20, "Supervisor Mode Access Prevention (SMAP)" },
    { 0x00000007, 0, REG_EBX,  7, "Supervisor-Mode Execution Prevention (SMEP)" },
    { 0x00000007, 0, REG_ECX, 13, "Total Memory Encryption (TME)" },
    { 0x00000007, 0, REG_ECX,  2, "User-mode Instruction Prevention (UMIP)" },
    { 0x00000007, 0, REG_EDX, 10, "VERW Buffer Overwrite (MD_CLEAR)" }
};

//--------------------------------------------------------------------------------------------------
// Power management features.
//--------------------------------------------------------------------------------------------------
constexpr CpuidFeatureBit kCpuidPowerManagement[] =
{
    { 0x80000007, 0, REG_EDX,  6, "100 MHz Multiplier Control" },
    { 0x00000006, 0, REG_EAX,  2, "Always Running APIC Timer (ARAT)" },
    { 0x00000006, 0, REG_EAX,  5, "Clock Modulation Duty Cycle Extension (ECMD)" },
    { 0x80000008, 0, REG_EBX, 27, "Collaborative Processor Performance Control (CPPC)" },
    { 0x80000007, 0, REG_EDX, 13, "Connected Standby" },
    { 0x80000007, 0, REG_EDX,  9, "Core Performance Boost (CPB)" },
    { 0x00000006, 0, REG_EAX,  0, "Digital Thermal Sensor (DTS)" },
    { 0x00000001, 0, REG_ECX,  7, "Enhanced SpeedStep Technology (EIST, ESS)" },
    { 0x80000007, 0, REG_EDX,  1, "Frequency Identification Control (FID)" },
    { 0x00000006, 0, REG_EAX, 13, "Hardware Duty Cycling (HDC)" },
    { 0x00000006, 0, REG_EAX, 19, "Hardware Feedback Interface" },
    { 0x80000007, 0, REG_EDX,  7, "Hardware P-State Control (HwPstate)" },
    { 0x00000006, 0, REG_EAX,  7, "Hardware P-States (HWP)" },
    { 0x80000007, 0, REG_EDX,  4, "Hardware Thermal Control (HTC)" },
    { 0x00000006, 0, REG_EAX,  9, "HWP Activity Window" },
    { 0x00000006, 0, REG_EAX, 15, "HWP Capabilities" },
    { 0x00000006, 0, REG_EAX, 10, "HWP Energy Performance Preference" },
    { 0x00000006, 0, REG_EAX,  8, "HWP Notification" },
    { 0x00000006, 0, REG_EAX, 11, "HWP Package Level Request" },
    { 0x80000007, 0, REG_EDX,  8, "Invariant Time Stamp Counter (TscInvariant)" },
    { 0x00000006, 0, REG_EAX,  6, "Package Thermal Management (PTM)" },
    { 0x00000006, 0, REG_EAX,  4, "Power Limit Notification (PLN)" },
    { 0x80000007, 0, REG_EDX, 11, "Processor Feedback Interface" },
    { 0x80000007, 0, REG_EDX, 12, "Processor Power Reporting" },
    { 0x80000007, 0, REG_EDX, 10, "Read-only Effective Frequency Interface (EffFreqRO)" },
    { 0x80000007, 0, REG_EDX, 14, "Running Average Power Limit (RAPL)" },
    { 0x80000007, 0, REG_EDX,  5, "Software Thermal Control (STC)" },
    { 0x80000007, 0, REG_EDX,  0, "Temperature Sensor (TS)" },
    { 0x00000001, 0, REG_EDX, 29, "Thermal Monitor (TM)" },
    { 0x00000001, 0, REG_ECX,  8, "Thermal Monitor 2 (TM2)" },
    { 0x80000007, 0, REG_EDX,  3, "Thermal Trip (TTP)" },
    { 0x00000006, 0, REG_EAX, 23, "Thread Director" },
    { 0x00000006, 0, REG_EAX, 14, "Turbo Boost Max Technology 3.0" },
    { 0x00000006, 0, REG_EAX,  1, "Turbo Boost Technology" },
    { 0x80000007, 0, REG_EDX,  2, "Voltage Identification Control (VID)" }
};

//--------------------------------------------------------------------------------------------------
// Virtualization features.
//
// The SVM sub-features are the EDX of leaf 8000000Ah and the memory encryption ones the EAX of
// leaf 8000001Fh. The Intel counterparts (EPT, VPID and the others) are not in CPUID at all
// but in the IA32_VMX MSRs, which only ring 0 reads.
//--------------------------------------------------------------------------------------------------
constexpr CpuidFeatureBit kCpuidVirtualization[] =
{
    { 0x8000000A, 0, REG_EDX, 13, "AMD Virtual Interrupt Controller (AVIC)" },
    { 0x8000000A, 0, REG_EDX,  7, "Decode Assists" },
    { 0x8000001F, 0, REG_EAX,  3, "Encrypted State (SEV-ES)" },
    { 0x8000000A, 0, REG_EDX,  6, "Flush by ASID" },
    { 0x8000000A, 0, REG_EDX, 17, "Guest Mode Execute Trap Extension (GMET)" },
    { 0x00000001, 0, REG_ECX, 31, "Hypervisor" },
    { 0x8000000A, 0, REG_EDX,  1, "LBR Virtualization" },
    { 0x80000020, 0, REG_EBX,  1, "Memory Bandwidth Enforcement (MBE)" },
    { 0x8000000A, 0, REG_EDX,  4, "MSR-Based TSC Rate Control" },
    { 0x8000000A, 0, REG_EDX,  0, "Nested Paging (NPT, RVI)" },
    { 0x8000000A, 0, REG_EDX, 25, "NMI Virtualization (vNMI)" },
    { 0x8000000A, 0, REG_EDX,  3, "NRIP Save (NRIPS)" },
    { 0x8000000A, 0, REG_EDX, 12, "PAUSE Filter Threshold" },
    { 0x8000000A, 0, REG_EDX, 10, "PAUSE Intercept Filter" },
    { 0x8000001F, 0, REG_EAX,  1, "Secure Encrypted Virtualization (SEV)" },
    { 0x8000001F, 0, REG_EAX,  4, "Secure Nested Paging (SEV-SNP)" },
    { 0x80000001, 0, REG_ECX,  2, "Secure Virtual Machine (SVM, Pacifica)" },
    { 0x8000000A, 0, REG_EDX, 20, "SPEC_CTRL Virtualization" },
    { 0x8000000A, 0, REG_EDX,  2, "SVM Lock (SVML)" },
    { 0x00000001, 0, REG_ECX,  5, "Virtual Machine Extensions (VMX, Vanderpool)" },
    { 0x8000001F, 0, REG_EAX, 16, "Virtual Transparent Encryption (VTE)" },
    { 0x8000000A, 0, REG_EDX, 16, "Virtualized GIF (vGIF)" },
    { 0x8000000A, 0, REG_EDX, 15, "Virtualized VMLOAD and VMSAVE" },
    { 0x8000000A, 0, REG_EDX,  5, "VMCB Clean Bits" },
    { 0x8000000A, 0, REG_EDX, 18, "x2APIC Virtualization (x2AVIC)" }
};

//--------------------------------------------------------------------------------------------------
// The rest of what the leaves report.
//--------------------------------------------------------------------------------------------------
constexpr CpuidFeatureBit kCpuidOther[] =
{
    { 0x80000001, 0, REG_EDX, 26, "1 GB Page Size" },
    { 0x00000001, 0, REG_EDX, 17, "36-bit Page Size Extension (PSE36)" },
    { 0x00000007, 0, REG_ECX, 16, "5-Level Paging (LA57)" },
    { 0x00000001, 0, REG_ECX,  2, "64-Bit Debug Store (DTES64)" },
    { 0x00000007, 0, REG_EDX, 19, "Architectural Last Branch Records (LBR)" },
    { 0x00000007, 1, REG_EAX,  8, "Architectural Performance Monitoring Extended" },
    { 0x00000007, 0, REG_ECX, 24, "Bus Lock Debug Exception" },
    { 0x00000010, 1, REG_ECX,  2, "Code and Data Prioritization (CDP)" },
    { 0x80000001, 0, REG_ECX, 23, "Core Performance Counters" },
    { 0x00000001, 0, REG_ECX,  4, "CPL Qualified Debug Store" },
    { 0x80000001, 0, REG_ECX, 26, "Data Breakpoint Extension" },
    { 0x00000001, 0, REG_EDX,  2, "Debug Extension (DE)" },
    { 0x00000001, 0, REG_EDX, 21, "Debug Store (DS)" },
    { 0x00000007, 0, REG_EBX, 13, "Deprecated FPU CS and FPU DS" },
    { 0x00000001, 0, REG_ECX, 18, "Direct Cache Access (DCA)" },
    { 0x80000001, 0, REG_ECX,  3, "Extended APIC Register Space" },
    { 0x0000000D, 1, REG_EAX,  4, "Extended Feature Disable (XFD)" },
    { 0x00000001, 0, REG_ECX, 21, "Extended xAPIC Support (x2APIC)" },
    { 0x00000001, 0, REG_EDX, 11, "Fast System Call (SEP)" },
    { 0x00000007, 1, REG_EAX, 17, "Flexible Return and Event Delivery (FRED)" },
    { 0x00000007, 0, REG_EDX, 15, "Hybrid Processor" },
    { 0x00000001, 0, REG_EDX, 28, "Hyper-Threading Technology (HTT)" },
    { 0x00000007, 0, REG_EDX, 29, "IA32_ARCH_CAPABILITIES MSR" },
    { 0x00000007, 0, REG_EDX, 30, "IA32_CORE_CAPABILITIES MSR" },
    { 0x80000001, 0, REG_ECX, 10, "Instruction Based Sampling" },
    { 0x80000008, 0, REG_EBX,  1, "Instructions Retired Counter" },
    { 0x00000007, 0, REG_EBX, 25, "Intel Processor Trace (PT)" },
    { 0x00000001, 0, REG_ECX, 10, "L1 Context ID" },
    { 0x80000001, 0, REG_ECX, 28, "Last Level Cache Performance Counters" },
    { 0x80000001, 0, REG_ECX, 15, "Light Weight Profiling" },
    { 0x00000001, 0, REG_EDX, 14, "Machine-Check Architecture (MCA)" },
    { 0x00000001, 0, REG_EDX,  7, "Machine-Check Exception (MCE)" },
    { 0x00000001, 0, REG_EDX, 12, "Memory Type Range Registers (MTRR)" },
    { 0x00000001, 0, REG_EDX,  5, "Model Specific Registers (MSR)" },
    { 0x80000001, 0, REG_ECX, 24, "NB Performance Counters" },
    { 0x00000001, 0, REG_EDX,  9, "On-chip APIC Hardware (APIC)" },
    { 0x00000001, 0, REG_ECX, 27, "OS-Enabled Extended State Management (OSXSAVE)" },
    { 0x00000001, 0, REG_EDX, 16, "Page Attribute Table (PAT)" },
    { 0x00000001, 0, REG_EDX, 13, "Page Global Enable (PGE)" },
    { 0x00000001, 0, REG_EDX,  3, "Page Size Extension (PSE)" },
    { 0x00000001, 0, REG_EDX, 31, "Pending Break Enable (PBE)" },
    { 0x00000001, 0, REG_ECX, 15, "Perfmon and Debug Capability" },
    { 0x80000001, 0, REG_ECX, 27, "Performance Time Stamp Counter (PTSC)" },
    { 0x00000001, 0, REG_EDX,  6, "Physical Address Extension (PAE)" },
    { 0x00000007, 0, REG_EDX, 18, "Platform Configuration (PCONFIG)" },
    { 0x00000007, 0, REG_EBX, 15, "Platform Quality of Service Enforcement (PQE)" },
    { 0x00000007, 0, REG_EBX, 12, "Platform Quality of Service Monitoring (PQM)" },
    { 0x00000001, 0, REG_ECX, 17, "Process Context Identifiers (PCID)" },
    { 0x80000008, 0, REG_EBX,  2, "Restore Error Pointers on XRSTOR" },
    { 0x00000001, 0, REG_EDX, 27, "Self-Snoop (SS)" },
    { 0x00000001, 0, REG_ECX, 11, "Silicon Debug Interface" },
    { 0x00000001, 0, REG_EDX, 22, "Thermal Monitor and Software Controlled Clock Facilities (ACPI)" },
    { 0x00000001, 0, REG_EDX,  4, "Time Stamp Counter (TSC)" },
    { 0x00000007, 0, REG_EBX,  1, "Time Stamp Counter Adjust (TSC_ADJUST)" },
    { 0x00000001, 0, REG_ECX, 24, "Time Stamp Counter Deadline" },
    { 0x80000001, 0, REG_ECX, 22, "Topology Extensions" },
    { 0x00000007, 0, REG_EBX,  4, "Transactional Synchronization Extensions (HLE)" },
    { 0x00000007, 0, REG_EBX, 11, "Transactional Synchronization Extensions (RTM)" },
    { 0x00000007, 0, REG_EDX, 16, "TSX Suspend Load Address Tracking (TSXLDTRK)" },
    { 0x80000021, 0, REG_EAX,  7, "Upper Address Ignore (UAI)" },
    { 0x00000007, 0, REG_EDX,  5, "User Interrupts (UINTR)" },
    { 0x00000001, 0, REG_EDX,  1, "Virtual Mode Extension (VME)" },
    { 0x80000001, 0, REG_ECX, 13, "Watchdog Timer" },
    { 0x0000000D, 1, REG_EAX,  2, "XGETBV with ECX = 1" },
    { 0x00000001, 0, REG_ECX, 14, "xTPR Update Control" },
    { 0x00000001, 0, REG_ECX, 26, "XSAVE / XSTOR States" },
    { 0x0000000D, 1, REG_EAX,  1, "XSAVEC and Compacted XRSTOR" },
    { 0x0000000D, 1, REG_EAX,  0, "XSAVEOPT Instruction" },
    { 0x0000000D, 1, REG_EAX,  3, "XSAVES / XRSTORS Instructions" }
};

//--------------------------------------------------------------------------------------------------
// The groups of features, in the order the report shows them.
constexpr struct
{
    CpuFeatureGroup group;
    std::span<const CpuidFeatureBit> features;
} kCpuidFeatureGroups[] =
{
    { GROUP_INSTRUCTION_SET, kCpuidInstructionSet   },
    { GROUP_SECURITY,        kCpuidSecurity         },
    { GROUP_POWER,           kCpuidPowerManagement  },
    { GROUP_VIRTUALIZATION,  kCpuidVirtualization   },
    { GROUP_OTHER,           kCpuidOther            }
};

//--------------------------------------------------------------------------------------------------
// The dump of the leaves, addressed the way the table above names them.
class Leafs
{
public:
    explicit Leafs(const QList<CpuidUtil::Leaf>& leafs)
    {
        for (const CpuidUtil::Leaf& item : leafs)
            leafs_.insert(key(item.leaf, item.subleaf), item);
    }

    bool contains(quint32 leaf, quint32 subleaf = 0) const
    {
        return leafs_.contains(key(leaf, subleaf));
    }

    quint32 value(quint32 leaf, quint32 subleaf, CpuidRegister reg) const
    {
        const auto it = leafs_.constFind(key(leaf, subleaf));
        if (it == leafs_.constEnd())
            return 0;

        switch (reg)
        {
            case REG_EAX: return it->eax;
            case REG_EBX: return it->ebx;
            case REG_ECX: return it->ecx;
            default:      return it->edx;
        }
    }

    bool bit(const CpuidFeatureBit& feature) const
    {
        return (value(feature.leaf, feature.subleaf, feature.reg) & (1u << feature.bit)) != 0;
    }

    // A string a leaf returns in its registers, four characters per register.
    QString string(quint32 leaf, std::initializer_list<CpuidRegister> registers) const
    {
        QByteArray buffer;

        for (CpuidRegister reg : registers)
        {
            const quint32 value_of_register = value(leaf, 0, reg);
            buffer.append(reinterpret_cast<const char*>(&value_of_register), sizeof(quint32));
        }

        const qsizetype end = buffer.indexOf('\0');
        if (end >= 0)
            buffer.truncate(end);

        return QString::fromLatin1(buffer).trimmed();
    }

private:
    static quint64 key(quint32 leaf, quint32 subleaf)
    {
        return (quint64(leaf) << 32) | subleaf;
    }

    QHash<quint64, CpuidUtil::Leaf> leafs_;
};

//--------------------------------------------------------------------------------------------------
QString toHexString(quint32 value, int digits)
{
    return QString::number(value, 16).toUpper().rightJustified(digits, QChar('0')) + QChar('h');
}

//--------------------------------------------------------------------------------------------------
void addIdentity(proto::system_info::Processor* processor, const char* name, const QString& value)
{
    proto::system_info::Processor::Identity* identity = processor->add_identity();
    identity->set_name(name);
    identity->set_value(value.toStdString());
}

//--------------------------------------------------------------------------------------------------
void fillCpuidIdentity(const Leafs& leafs, proto::system_info::Processor* processor)
{
    const QString vendor = leafs.string(0x00000000, { REG_EBX, REG_EDX, REG_ECX });
    if (!vendor.isEmpty())
        addIdentity(processor, "CPUID Vendor", vendor);

    // The brand string is spread over three leaves, four registers each.
    QString brand;
    for (quint32 leaf = 0x80000002; leaf <= 0x80000004; ++leaf)
        brand += leafs.string(leaf, { REG_EAX, REG_EBX, REG_ECX, REG_EDX });

    if (!brand.isEmpty())
        addIdentity(processor, "CPUID CPU Name", brand.simplified());

    if (leafs.contains(0x00000001))
    {
        const quint32 signature = leafs.value(0x00000001, 0, REG_EAX);

        const quint32 base_family = (signature >> 8) & 0xF;
        const quint32 base_model = (signature >> 4) & 0xF;
        const quint32 extended_family = (signature >> 20) & 0xFF;
        const quint32 extended_model = (signature >> 16) & 0xF;

        addIdentity(processor, "CPUID Version", toHexString(signature, 8));

        if (leafs.contains(0x80000001))
        {
            addIdentity(processor, "Extended CPUID Version",
                        toHexString(leafs.value(0x80000001, 0, REG_EAX), 8));
        }

        // The extended fields are only added to the ones they extend, and only the values that say
        // the fields are in use call for them.
        quint32 family = base_family;
        quint32 model = base_model;

        if (base_family == 0xF)
            family += extended_family;

        if (base_family == 0x6 || base_family == 0xF)
            model += extended_model << 4;

        addIdentity(processor, "Family", toHexString(family, 2));
        addIdentity(processor, "Model", toHexString(model, 2));
        addIdentity(processor, "Stepping", toHexString(signature & 0xF, 2));

        if (extended_family)
            addIdentity(processor, "Extended Family", toHexString(extended_family, 2));

        if (extended_model)
            addIdentity(processor, "Extended Model", toHexString(extended_model, 2));

        addIdentity(processor, "Brand ID", toHexString(leafs.value(0x00000001, 0, REG_EBX) & 0xFF, 2));
    }

    // The frequencies are only reported by a processor that supports the leaf.
    if (leafs.contains(0x00000016))
    {
        const quint32 base = leafs.value(0x00000016, 0, REG_EAX) & 0xFFFF;
        const quint32 maximum = leafs.value(0x00000016, 0, REG_EBX) & 0xFFFF;
        const quint32 bus = leafs.value(0x00000016, 0, REG_ECX) & 0xFFFF;

        if (base)
            addIdentity(processor, "Base Frequency", QString("%1 MHz").arg(base));

        if (maximum)
            addIdentity(processor, "Max Frequency", QString("%1 MHz").arg(maximum));

        if (bus)
            addIdentity(processor, "Bus Frequency", QString("%1 MHz").arg(bus));
    }

    // The leaf is only defined when the processor runs under a hypervisor.
    const QString hypervisor = leafs.string(0x40000000, { REG_EBX, REG_ECX, REG_EDX });
    if (!hypervisor.isEmpty())
        addIdentity(processor, "Hypervisor", hypervisor);
}

//--------------------------------------------------------------------------------------------------
void fillCpuidCaches(const Leafs& leafs, proto::system_info::Processor* processor)
{
    // Both vendors describe the caches the same way, Intel under a standard leaf and AMD under an
    // extended one.
    quint32 cache_leaf = 0;

    if (leafs.value(0x00000004, 0, REG_EAX) & 0x1F)
        cache_leaf = 0x00000004;
    else if (leafs.value(0x8000001D, 0, REG_EAX) & 0x1F)
        cache_leaf = 0x8000001D;

    if (!cache_leaf)
        return;

    for (quint32 subleaf = 0; leafs.contains(cache_leaf, subleaf); ++subleaf)
    {
        const quint32 eax = leafs.value(cache_leaf, subleaf, REG_EAX);
        const quint32 ebx = leafs.value(cache_leaf, subleaf, REG_EBX);

        const quint32 type = eax & 0x1F;
        if (!type)
            break;

        const quint32 ways = ((ebx >> 22) & 0x3FF) + 1;
        const quint32 partitions = ((ebx >> 12) & 0x3FF) + 1;
        const quint32 line_size = (ebx & 0xFFF) + 1;
        const quint32 sets = leafs.value(cache_leaf, subleaf, REG_ECX) + 1;

        proto::system_info::Processor::Cache* cache = processor->add_cache();

        switch (type)
        {
            case 1:
                cache->set_type(proto::system_info::Processor::Cache::TYPE_DATA);
                break;

            case 2:
                cache->set_type(proto::system_info::Processor::Cache::TYPE_INSTRUCTION);
                break;

            case 3:
                cache->set_type(proto::system_info::Processor::Cache::TYPE_UNIFIED);
                break;

            default:
                break;
        }

        cache->set_level((eax >> 5) & 0x7);
        cache->set_size(quint64(ways) * partitions * line_size * sets);
        cache->set_ways(ways);
        cache->set_line_size(line_size);
        cache->set_sets(sets);
        cache->set_threads(((eax >> 14) & 0xFFF) + 1);

        // A fully associative cache says so with a flag of its own instead of a number of ways.
        cache->set_fully_associative((eax & (1u << 9)) != 0);
    }
}

//--------------------------------------------------------------------------------------------------
void fillCpuidFeatures(const Leafs& leafs, proto::system_info::Processor* processor)
{
    for (const auto& list : kCpuidFeatureGroups)
    {
        // A feature listed with more than one source is there when any of its bits is set.
        QStringList names;
        QHash<QString, bool> supported;

        for (const CpuidFeatureBit& item : list.features)
        {
            // A bit of a leaf the processor does not answer says nothing about the feature.
            if (!leafs.contains(item.leaf, item.subleaf))
                continue;

            const QString name = QString::fromLatin1(item.name);

            if (!supported.contains(name))
                names << name;

            supported[name] = supported.value(name) || leafs.bit(item);
        }

        // A feature is looked up by its name, and the order the vendors put the bits in is not one.
        names.sort(Qt::CaseInsensitive);

        for (const QString& name : std::as_const(names))
        {
            proto::system_info::Processor::Feature* feature = addFeature(processor, list.group);

            feature->set_name(name.toStdString());
            feature->set_supported(supported.value(name));
        }
    }
}

#endif // defined(Q_PROCESSOR_X86)

// The identification registers of ARM64 answer what CPUID answers on x86, a field of four bits
// per capability instead of a bit. The registers are privileged, so each system hands them over
// its own way, and the report is made of the same kind of named features as on x86.
#if defined(Q_PROCESSOR_ARM_64)

// The ID_AA64*_EL1 registers the fields live in.
enum ArmRegister
{
    REG_PFR0 = 0,
    REG_PFR1,
    REG_ZFR0,
    REG_ISAR0,
    REG_ISAR1,
    REG_ISAR2,
    REG_MMFR0,
    REG_MMFR1,
    REG_MMFR2,
    REG_COUNT
};

// A field of an identification register and the feature a large enough value of it reports. On
// macOS the registers are out of reach and the sysctl key stands in for the field: a full name is
// asked as it is, a bare one with the "hw.optional.arm." prefix, and a feature the system has no
// key for stays off the report there.
struct ArmFeatureBit
{
    ArmRegister reg;
    int shift;
    int min;
    const char* sysctl;
    const char* name;
};

//--------------------------------------------------------------------------------------------------
// Instruction set.
//--------------------------------------------------------------------------------------------------
constexpr ArmFeatureBit kArmInstructionSet[] =
{
    { REG_ISAR0, 20, 3, "FEAT_LSE128", "128-bit Atomic Instructions (FEAT_LSE128)" },
    { REG_PFR0,  20, 0, "hw.optional.AdvSIMD", "Advanced SIMD (NEON)" },
    { REG_ISAR0,  4, 1, "FEAT_AES", "AES Instructions (FEAT_AES)" },
    { REG_ISAR1, 44, 1, "FEAT_BF16", "BFloat16 Instructions (FEAT_BF16)" },
    { REG_ISAR2, 52, 1, "FEAT_CSSC", "Common Short Sequence Compression (FEAT_CSSC)" },
    { REG_ISAR1, 16, 1, "FEAT_FCMA", "Complex Number Instructions (FEAT_FCMA)" },
    { REG_ISAR0, 16, 1, "FEAT_CRC32", "CRC32 Instructions (FEAT_CRC32)" },
    { REG_ISAR1,  0, 2, "FEAT_DPB2", "Data Cache Clean to PoDP (FEAT_DPB2)" },
    { REG_ISAR1,  0, 1, "FEAT_DPB", "Data Cache Clean to PoP (FEAT_DPB)" },
    { REG_ISAR1, 48, 1, "FEAT_DGH", "Data Gathering Hint (FEAT_DGH)" },
    { REG_ISAR0, 44, 1, "FEAT_DotProd", "Dot Product Instructions (FEAT_DotProd)" },
    { REG_ISAR1, 44, 2, "FEAT_EBF16", "Extended BFloat16 (FEAT_EBF16)" },
    { REG_ISAR0, 52, 1, "FEAT_FlagM", "Flag Manipulation (FEAT_FlagM)" },
    { REG_ISAR0, 52, 2, "FEAT_FlagM2", "Flag Manipulation 2 (FEAT_FlagM2)" },
    { REG_PFR0,  16, 0, "hw.optional.floatingpoint", "Floating Point (FP)" },
    { REG_ISAR1, 32, 1, "FEAT_FRINTTS", "FP Round to Integral (FEAT_FRINTTS)" },
    { REG_ISAR0, 48, 1, "FEAT_FHM", "FP16 Multiply Accumulate (FEAT_FHM)" },
    { REG_PFR0,  16, 1, "FEAT_FP16", "Half-Precision Floating Point (FEAT_FP16)" },
    { REG_ISAR2, 20, 1, "FEAT_HBC", "Hinted Conditional Branches (FEAT_HBC)" },
    { REG_ISAR1, 52, 1, "FEAT_I8MM", "Int8 Matrix Multiplication (FEAT_I8MM)" },
    { REG_ISAR1, 12, 1, "FEAT_JSCVT", "JavaScript Conversion (FEAT_JSCVT)" },
    { REG_ISAR0, 20, 2, "FEAT_LSE", "Large System Extensions (FEAT_LSE)" },
    { REG_MMFR2, 32, 1, "FEAT_LSE2", "Large System Extensions 2 (FEAT_LSE2)" },
    { REG_ISAR1, 20, 1, "FEAT_LRCPC", "Load-Acquire RCpc (FEAT_LRCPC)" },
    { REG_ISAR1, 20, 2, "FEAT_LRCPC2", "Load-Acquire RCpc 2 (FEAT_LRCPC2)" },
    { REG_ISAR2, 16, 1, "FEAT_MOPS", "Memory Copy Instructions (FEAT_MOPS)" },
    { REG_ISAR0,  4, 2, "FEAT_PMULL", "Polynomial Multiply Long (FEAT_PMULL)" },
    { REG_ISAR0, 60, 1, "FEAT_RNG", "Random Number Generation (FEAT_RNG)" },
    { REG_ISAR0, 28, 1, "FEAT_RDM", "Rounding Double Multiply (FEAT_RDM)" },
    { REG_PFR1,  24, 1, "FEAT_SME", "Scalable Matrix Extension (SME)" },
    { REG_PFR1,  24, 2, "FEAT_SME2", "Scalable Matrix Extension 2 (SME2)" },
    { REG_PFR0,  32, 1, "FEAT_SVE", "Scalable Vector Extension (SVE)" },
    { REG_ZFR0,   0, 1, "FEAT_SVE2", "Scalable Vector Extension 2 (SVE2)" },
    { REG_ISAR0,  8, 1, "FEAT_SHA1", "SHA1 Instructions (FEAT_SHA1)" },
    { REG_ISAR0, 12, 1, "FEAT_SHA256", "SHA256 Instructions (FEAT_SHA256)" },
    { REG_ISAR0, 32, 1, "FEAT_SHA3", "SHA3 Instructions (FEAT_SHA3)" },
    { REG_ISAR0, 12, 2, "FEAT_SHA512", "SHA512 Instructions (FEAT_SHA512)" },
    { REG_ISAR0, 36, 1, "FEAT_SM3", "SM3 Instructions (FEAT_SM3)" },
    { REG_ISAR0, 40, 1, "FEAT_SM4", "SM4 Instructions (FEAT_SM4)" },
    { REG_ISAR0, 24, 1, "FEAT_TME", "Transactional Memory Extension (FEAT_TME)" },
    { REG_ISAR2,  0, 2, "FEAT_WFxT", "WFE and WFI with Timeout (FEAT_WFxT)" }
};

//--------------------------------------------------------------------------------------------------
// Security features.
//--------------------------------------------------------------------------------------------------
constexpr ArmFeatureBit kArmSecurity[] =
{
    { REG_PFR1,   0, 1, "FEAT_BTI", "Branch Target Identification (FEAT_BTI)" },
    { REG_PFR0,  56, 1, "FEAT_CSV2", "Cache Speculation Variant 2 (FEAT_CSV2)" },
    { REG_PFR0,  60, 1, "FEAT_CSV3", "Cache Speculation Variant 3 (FEAT_CSV3)" },
    { REG_PFR0,  48, 1, "FEAT_DIT", "Data Independent Timing (FEAT_DIT)" },
    // Pointer authentication is reported once per algorithm, each in a field of its own. Rows
    // under the same name are one feature with a source per algorithm.
    { REG_ISAR1,  4, 4, "FEAT_FPAC", "Faulting Pointer Authentication (FEAT_FPAC)" },
    { REG_ISAR1,  8, 4, "FEAT_FPAC", "Faulting Pointer Authentication (FEAT_FPAC)" },
    { REG_ISAR2, 12, 4, "FEAT_FPAC", "Faulting Pointer Authentication (FEAT_FPAC)" },
    { REG_ISAR1, 24, 1, nullptr, "Generic Pointer Authentication (PACGA)" },
    { REG_ISAR1, 28, 1, nullptr, "Generic Pointer Authentication (PACGA)" },
    { REG_ISAR2,  8, 1, nullptr, "Generic Pointer Authentication (PACGA)" },
    { REG_PFR1,   8, 1, "FEAT_MTE", "Memory Tagging Extension (FEAT_MTE)" },
    { REG_ISAR1,  4, 1, "FEAT_PAuth", "Pointer Authentication (FEAT_PAuth)" },
    { REG_ISAR1,  8, 1, "FEAT_PAuth", "Pointer Authentication (FEAT_PAuth)" },
    { REG_ISAR2, 12, 1, "FEAT_PAuth", "Pointer Authentication (FEAT_PAuth)" },
    { REG_ISAR1,  4, 3, "FEAT_PAuth2", "Pointer Authentication 2 (FEAT_PAuth2)" },
    { REG_ISAR1,  8, 3, "FEAT_PAuth2", "Pointer Authentication 2 (FEAT_PAuth2)" },
    { REG_ISAR2, 12, 3, "FEAT_PAuth2", "Pointer Authentication 2 (FEAT_PAuth2)" },
    { REG_ISAR1, 40, 1, "FEAT_SPECRES", "Prediction Invalidation (FEAT_SPECRES)" },
    { REG_ISAR1, 36, 1, "FEAT_SB", "Speculation Barrier (FEAT_SB)" },
    { REG_PFR1,   4, 1, "FEAT_SSBS", "Speculative Store Bypass Safe (FEAT_SSBS)" }
};

//--------------------------------------------------------------------------------------------------
// The rest of what the registers report.
//--------------------------------------------------------------------------------------------------
constexpr ArmFeatureBit kArmOther[] =
{
    { REG_MMFR1, 44, 1, "FEAT_AFP", "Alternate Floating-Point Behavior (FEAT_AFP)" },
    { REG_MMFR0, 60, 1, "FEAT_ECV", "Enhanced Counter Virtualization (FEAT_ECV)" },
    { REG_ISAR2,  4, 1, "FEAT_RPRES", "Reciprocal Estimate Precision (FEAT_RPRES)" }
};

//--------------------------------------------------------------------------------------------------
// The groups of features, in the order the report shows them.
constexpr struct
{
    CpuFeatureGroup group;
    std::span<const ArmFeatureBit> features;
} kArmFeatureGroups[] =
{
    { GROUP_INSTRUCTION_SET, kArmInstructionSet },
    { GROUP_SECURITY,        kArmSecurity      },
    { GROUP_OTHER,           kArmOther         }
};

#if defined(Q_OS_LINUX)
//--------------------------------------------------------------------------------------------------
// The registers are privileged, but the kernel traps the reads and answers them itself, with the
// fields it does not stand behind cleared. Whether it does so is a capability of its own.
bool readArmRegisters(quint64* regs, bool* present)
{
    constexpr unsigned long kHwcapCpuid = 1ul << 11;

    if (!(getauxval(AT_HWCAP) & kHwcapCpuid))
        return false;

    // The registers are spelled by their encodings: those need no support from the assembler.
    asm("mrs %0, S3_0_C0_C4_0" : "=r"(regs[REG_PFR0]));   // ID_AA64PFR0_EL1
    asm("mrs %0, S3_0_C0_C4_1" : "=r"(regs[REG_PFR1]));   // ID_AA64PFR1_EL1
    asm("mrs %0, S3_0_C0_C4_4" : "=r"(regs[REG_ZFR0]));   // ID_AA64ZFR0_EL1
    asm("mrs %0, S3_0_C0_C6_0" : "=r"(regs[REG_ISAR0]));  // ID_AA64ISAR0_EL1
    asm("mrs %0, S3_0_C0_C6_1" : "=r"(regs[REG_ISAR1]));  // ID_AA64ISAR1_EL1
    asm("mrs %0, S3_0_C0_C6_2" : "=r"(regs[REG_ISAR2]));  // ID_AA64ISAR2_EL1
    asm("mrs %0, S3_0_C0_C7_0" : "=r"(regs[REG_MMFR0]));  // ID_AA64MMFR0_EL1
    asm("mrs %0, S3_0_C0_C7_1" : "=r"(regs[REG_MMFR1]));  // ID_AA64MMFR1_EL1
    asm("mrs %0, S3_0_C0_C7_2" : "=r"(regs[REG_MMFR2]));  // ID_AA64MMFR2_EL1

    for (int i = 0; i < REG_COUNT; ++i)
        present[i] = true;

    return true;
}

#elif defined(Q_OS_WINDOWS)
//--------------------------------------------------------------------------------------------------
// The system reads the registers at boot and mirrors them in the registry, a value per register,
// named by the encoding of the register.
bool readArmRegisters(quint64* regs, bool* present)
{
    constexpr struct
    {
        ArmRegister reg;
        const wchar_t* name;
    } kValues[] =
    {
        { REG_PFR0,  L"CP 4020" },
        { REG_PFR1,  L"CP 4021" },
        { REG_ZFR0,  L"CP 4024" },
        { REG_ISAR0, L"CP 4030" },
        { REG_ISAR1, L"CP 4031" },
        { REG_ISAR2, L"CP 4032" },
        { REG_MMFR0, L"CP 4038" },
        { REG_MMFR1, L"CP 4039" },
        { REG_MMFR2, L"CP 403A" }
    };

    bool any = false;

    for (const auto& item : kValues)
    {
        quint64 value = 0;
        DWORD size = sizeof(value);

        if (RegGetValueW(HKEY_LOCAL_MACHINE,
                         L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
                         item.name, RRF_RT_REG_QWORD, nullptr, &value, &size) != ERROR_SUCCESS)
        {
            continue;
        }

        regs[item.reg] = value;
        present[item.reg] = true;
        any = true;
    }

    return any;
}
#endif

//--------------------------------------------------------------------------------------------------
void fillArmFeatures(proto::system_info::Processor* processor)
{
#if !defined(Q_OS_MACOS)
    quint64 regs[REG_COUNT] = {};
    bool present[REG_COUNT] = {};

    if (!readArmRegisters(regs, present))
        return;
#endif

    for (const auto& list : kArmFeatureGroups)
    {
        // A feature listed with more than one source is there when any of its fields says so.
        QStringList names;
        QHash<QString, bool> supported;

        for (const ArmFeatureBit& item : list.features)
        {
#if defined(Q_OS_MACOS)
            if (!item.sysctl)
                continue;

            QByteArray key(item.sysctl);
            if (!key.startsWith("hw."))
                key.prepend("hw.optional.arm.");

            // A key the system does not know is a feature the processor does not have.
            int value = 0;
            size_t size = sizeof(value);
            const bool on =
                sysctlbyname(key.constData(), &value, &size, nullptr, 0) == 0 && value != 0;
#else
            // A field of a register the system did not hand over says nothing about the feature.
            if (!present[item.reg])
                continue;

            // The fields that can say "not implemented" say it with 0Fh, and no field defined so
            // far reaches that as a plain value.
            const quint32 field = quint32(regs[item.reg] >> item.shift) & 0xF;
            const bool on = field != 0xF && field >= quint32(item.min);
#endif

            const QString name = QString::fromLatin1(item.name);
            if (!supported.contains(name))
                names << name;

            supported[name] = supported.value(name) || on;
        }

        // A feature is looked up by its name, and the order of the fields is not one.
        names.sort(Qt::CaseInsensitive);

        for (const QString& name : std::as_const(names))
        {
            proto::system_info::Processor::Feature* feature = addFeature(processor, list.group);
            feature->set_name(name.toStdString());
            feature->set_supported(supported.value(name));
        }
    }
}

#endif // defined(Q_PROCESSOR_ARM_64)

} // namespace

//--------------------------------------------------------------------------------------------------
void fillProcessorInfo(proto::system_info::Processor* processor)
{
    processor->set_vendor(SysInfo::processorVendor().toStdString());
    processor->set_model(SysInfo::processorName().toStdString());
    processor->set_packages(static_cast<quint32>(SysInfo::processorPackages()));
    processor->set_cores(static_cast<quint32>(SysInfo::processorCores()));
    processor->set_threads(static_cast<quint32>(SysInfo::processorThreads()));

#if defined(Q_PROCESSOR_X86)
    const Leafs leafs(CpuidUtil::dump());

    fillCpuidIdentity(leafs, processor);
    fillCpuidCaches(leafs, processor);
    fillCpuidFeatures(leafs, processor);
#elif defined(Q_PROCESSOR_ARM_64)
    fillArmFeatures(processor);
#endif
}
