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

#include "common/sys_info/sys_info_widget_cpu.h"

#include <QHash>
#include <QMenu>

#include <algorithm>
#include <iterator>

#include "common/system_info_constants.h"
#include "common/desktop/formatter.h"
#include "proto/system_info.h"
#include "ui_sys_info_widget_cpu.h"

namespace {

const char kCpuIcon[] = ":/img/microchip.svg";

enum Register
{
    REG_EAX,
    REG_EBX,
    REG_ECX,
    REG_EDX
};

// Groups the features are shown under. A bit is put where it is looked for and not where the
// vendor happened to place it in the registers.
enum Group
{
    GROUP_INSTRUCTION_SET = 0,
    GROUP_SECURITY,
    GROUP_POWER,
    GROUP_VIRTUALIZATION,
    GROUP_CPUID,
    GROUP_COUNT
};

// A bit of a CPUID register and what the processor says with it.
struct FeatureBit
{
    Group group;
    const char* name;
    quint32 leaf;
    quint32 subleaf;
    Register reg;
    int bit;
};

//--------------------------------------------------------------------------------------------------
constexpr FeatureBit kFeatures[] =
{
    //----------------------------------------------------------------------------------------------
    // Instruction set.
    //----------------------------------------------------------------------------------------------
    { GROUP_INSTRUCTION_SET, "64-bit x86 Extension (AMD64, Intel64)", 0x80000001, 0, REG_EDX, 29 },
    { GROUP_INSTRUCTION_SET, "AES Instruction (AES)", 0x00000001, 0, REG_ECX, 25 },
    { GROUP_INSTRUCTION_SET, "AES Instruction Set (VEX-256/EVEX)", 0x00000007, 0, REG_ECX, 9 },
    { GROUP_INSTRUCTION_SET, "AMD 3DNow!", 0x80000001, 0, REG_EDX, 31 },
    { GROUP_INSTRUCTION_SET, "AMD 3DNowPrefetch", 0x80000001, 0, REG_ECX, 8 },
    { GROUP_INSTRUCTION_SET, "AMD Extended 3DNow!", 0x80000001, 0, REG_EDX, 30 },
    { GROUP_INSTRUCTION_SET, "AMD Extended MMX", 0x80000001, 0, REG_EDX, 22 },
    { GROUP_INSTRUCTION_SET, "AMD FMA4", 0x80000001, 0, REG_ECX, 16 },
    { GROUP_INSTRUCTION_SET, "AMD MisAligned SSE", 0x80000001, 0, REG_ECX, 7 },
    { GROUP_INSTRUCTION_SET, "AMD SSE4A", 0x80000001, 0, REG_ECX, 6 },
    { GROUP_INSTRUCTION_SET, "AMD XOP", 0x80000001, 0, REG_ECX, 11 },
    { GROUP_INSTRUCTION_SET, "Advanced Vector Extension (AVX)", 0x00000001, 0, REG_ECX, 28 },
    { GROUP_INSTRUCTION_SET, "Advanced Vector Extensions 2 (AVX2)", 0x00000007, 0, REG_EBX, 5 },
    { GROUP_INSTRUCTION_SET, "Advanced Performance Extensions (APX_F)", 0x00000007, 1, REG_EDX, 21 },
    { GROUP_INSTRUCTION_SET, "AVX-512 4-register Multiply Accumulation Single Precision "
      "(AVX5124FMAPS)", 0x00000007, 0, REG_EDX, 3 },
    { GROUP_INSTRUCTION_SET, "AVX-512 4-register Neural Network Instructions (AVX5124VNNIW)",
      0x00000007, 0, REG_EDX, 2 },
    { GROUP_INSTRUCTION_SET, "AVX-512 BITALG Instructions (AVX512BITALG)",
      0x00000007, 0, REG_ECX, 12 },
    { GROUP_INSTRUCTION_SET, "AVX-512 Byte and Word Instructions (AVX512BW)",
      0x00000007, 0, REG_EBX, 30 },
    { GROUP_INSTRUCTION_SET, "AVX-512 Conflict Detection Instructions (AVX512CD)",
      0x00000007, 0, REG_EBX, 28 },
    { GROUP_INSTRUCTION_SET, "AVX-512 Doubleword and Quadword Instructions (AVX512DQ)",
      0x00000007, 0, REG_EBX, 17 },
    { GROUP_INSTRUCTION_SET, "AVX-512 Exponential and Reciprocal Instructions (AVX512ER)",
      0x00000007, 0, REG_EBX, 27 },
    { GROUP_INSTRUCTION_SET, "AVX-512 Foundation (AVX512F)", 0x00000007, 0, REG_EBX, 16 },
    { GROUP_INSTRUCTION_SET, "AVX-512 Half-Precision (AVX512_FP16)", 0x00000007, 0, REG_EDX, 23 },
    { GROUP_INSTRUCTION_SET, "AVX-512 Integer Fused Multiply-Add Instructions (AVX512IFMA)",
      0x00000007, 0, REG_EBX, 21 },
    { GROUP_INSTRUCTION_SET, "AVX-512 Prefetch Instructions (AVX512PF)",
      0x00000007, 0, REG_EBX, 26 },
    { GROUP_INSTRUCTION_SET, "AVX-512 Vector Bit Manipulation Instructions (AVX512VBMI)",
      0x00000007, 0, REG_ECX, 1 },
    { GROUP_INSTRUCTION_SET, "AVX-512 Vector Bit Manipulation Instructions 2 (AVX512VBMI2)",
      0x00000007, 0, REG_ECX, 6 },
    { GROUP_INSTRUCTION_SET, "AVX-512 Vector Length Extensions (AVX512VL)",
      0x00000007, 0, REG_EBX, 31 },
    { GROUP_INSTRUCTION_SET, "AVX-512 Vector Neural Network Instructions (AVX512VNNI)",
      0x00000007, 0, REG_ECX, 11 },
    { GROUP_INSTRUCTION_SET, "AVX-512 Vector Pair Intersection (AVX512_VP2INTERSECT)",
      0x00000007, 0, REG_EDX, 8 },
    { GROUP_INSTRUCTION_SET, "AVX-512 Vector Population Count D/Q (AVX512VPOPCNTDQ)",
      0x00000007, 0, REG_ECX, 14 },
    { GROUP_INSTRUCTION_SET, "AVX-512 bfloat16 (AVX512_BF16)", 0x00000007, 1, REG_EAX, 5 },
    { GROUP_INSTRUCTION_SET, "AVX Integer Fused Multiply-Add (AVX-IFMA)",
      0x00000007, 1, REG_EAX, 23 },
    { GROUP_INSTRUCTION_SET, "AVX Vector Neural Network Instructions (AVX-VNNI)",
      0x00000007, 1, REG_EAX, 4 },
    { GROUP_INSTRUCTION_SET, "AVX-NE-CONVERT", 0x00000007, 1, REG_EDX, 5 },
    { GROUP_INSTRUCTION_SET, "AVX-VNNI-INT8", 0x00000007, 1, REG_EDX, 4 },
    { GROUP_INSTRUCTION_SET, "AVX10", 0x00000007, 1, REG_EDX, 19 },
    { GROUP_INSTRUCTION_SET, "Advanced Matrix Extensions (AMX-TILE)", 0x00000007, 0, REG_EDX, 24 },
    { GROUP_INSTRUCTION_SET, "AMX Computation on 8-bit Integers (AMX-INT8)",
      0x00000007, 0, REG_EDX, 25 },
    { GROUP_INSTRUCTION_SET, "AMX Computation on bfloat16 (AMX-BF16)",
      0x00000007, 0, REG_EDX, 22 },
    { GROUP_INSTRUCTION_SET, "AMX Computation on Complex Numbers (AMX-COMPLEX)",
      0x00000007, 1, REG_EDX, 8 },
    { GROUP_INSTRUCTION_SET, "AMX Computation on Half-Precision (AMX-FP16)",
      0x00000007, 1, REG_EAX, 21 },
    { GROUP_INSTRUCTION_SET, "Bit Manipulation Instruction Set 1 (BMI1)",
      0x00000007, 0, REG_EBX, 3 },
    { GROUP_INSTRUCTION_SET, "Bit Manipulation Instruction Set 2 (BMI2)",
      0x00000007, 0, REG_EBX, 8 },
    { GROUP_INSTRUCTION_SET, "CLDEMOTE Instruction", 0x00000007, 0, REG_ECX, 25 },
    { GROUP_INSTRUCTION_SET, "CLFLUSH Instruction", 0x00000001, 0, REG_EDX, 19 },
    { GROUP_INSTRUCTION_SET, "CLFLUSHOPT Instruction", 0x00000007, 0, REG_EBX, 23 },
    { GROUP_INSTRUCTION_SET, "CLMUL Instruction Set (VEX-256/EVEX)", 0x00000007, 0, REG_ECX, 10 },
    { GROUP_INSTRUCTION_SET, "CLWB Instruction", 0x00000007, 0, REG_EBX, 24 },
    { GROUP_INSTRUCTION_SET, "CLZERO Instruction", 0x80000008, 0, REG_EBX, 0 },
    { GROUP_INSTRUCTION_SET, "CMPCCXADD Instruction", 0x00000007, 1, REG_EAX, 7 },
    { GROUP_INSTRUCTION_SET, "CMPXCHG8B Instruction", 0x00000001, 0, REG_EDX, 8 },
    { GROUP_INSTRUCTION_SET, "CMPXCHG16B Instruction", 0x00000001, 0, REG_ECX, 13 },
    { GROUP_INSTRUCTION_SET, "Conditional Move Instruction (CMOV)", 0x00000001, 0, REG_EDX, 15 },
    { GROUP_INSTRUCTION_SET, "Enhanced REP MOVSB/STOSB", 0x00000007, 0, REG_EBX, 9 },
    { GROUP_INSTRUCTION_SET, "Enqueue Stores (ENQCMD)", 0x00000007, 0, REG_ECX, 29 },
    { GROUP_INSTRUCTION_SET, "Fast Short REP CMPSB/SCASB (FSRC)", 0x00000007, 1, REG_EAX, 12 },
    { GROUP_INSTRUCTION_SET, "Fast Short REP MOVSB (FSRM)", 0x00000007, 0, REG_EDX, 4 },
    { GROUP_INSTRUCTION_SET, "Fast Short REP STOSB (FSRS)", 0x00000007, 1, REG_EAX, 11 },
    { GROUP_INSTRUCTION_SET, "Fast Zero-Length REP MOVSB (FZRM)", 0x00000007, 1, REG_EAX, 10 },
    { GROUP_INSTRUCTION_SET, "Float-16-bit Conversion Instructions (F16C)",
      0x00000001, 0, REG_ECX, 29 },
    { GROUP_INSTRUCTION_SET, "Floating-point Unit On-Chip (FPU)", 0x00000001, 0, REG_EDX, 0 },
    { GROUP_INSTRUCTION_SET, "Fused Multiply Add (FMA)", 0x00000001, 0, REG_ECX, 12 },
    { GROUP_INSTRUCTION_SET, "FXSAVE / FXSTOR Instruction", 0x00000001, 0, REG_EDX, 24 },
    { GROUP_INSTRUCTION_SET, "Galois Field Instructions (GFNI)", 0x00000007, 0, REG_ECX, 8 },
    { GROUP_INSTRUCTION_SET, "HRESET Instruction", 0x00000007, 1, REG_EAX, 22 },
    { GROUP_INSTRUCTION_SET, "IA-64", 0x00000001, 0, REG_EDX, 30 },
    { GROUP_INSTRUCTION_SET, "INVPCID Instruction", 0x00000007, 0, REG_EBX, 10 },
    { GROUP_INSTRUCTION_SET, "LAHF / SAHF Instruction", 0x80000001, 0, REG_ECX, 0 },
    { GROUP_INSTRUCTION_SET, "LKGS Instruction", 0x00000007, 1, REG_EAX, 18 },
    { GROUP_INSTRUCTION_SET, "LZCNT Instruction", 0x80000001, 0, REG_ECX, 5 },
    { GROUP_INSTRUCTION_SET, "MCOMMIT Instruction", 0x80000008, 0, REG_EBX, 8 },
    { GROUP_INSTRUCTION_SET, "MMX Technology (MMX)", 0x00000001, 0, REG_EDX, 23 },
    { GROUP_INSTRUCTION_SET, "MONITOR / MWAIT Instruction", 0x00000001, 0, REG_ECX, 3 },
    { GROUP_INSTRUCTION_SET, "MONITORX / MWAITX Instruction", 0x80000001, 0, REG_ECX, 29 },
    { GROUP_INSTRUCTION_SET, "MOVBE Instruction", 0x00000001, 0, REG_ECX, 22 },
    { GROUP_INSTRUCTION_SET, "MOVDIR64B Instruction", 0x00000007, 0, REG_ECX, 28 },
    { GROUP_INSTRUCTION_SET, "MOVDIRI Instruction", 0x00000007, 0, REG_ECX, 27 },
    { GROUP_INSTRUCTION_SET, "MSRLIST Instruction", 0x00000007, 1, REG_EAX, 27 },
    { GROUP_INSTRUCTION_SET, "Multi-Precision Add-Carry Instruction Extensions (ADX)",
      0x00000007, 0, REG_EBX, 19 },
    { GROUP_INSTRUCTION_SET, "PCLMULDQ Instruction", 0x00000001, 0, REG_ECX, 1 },
    { GROUP_INSTRUCTION_SET, "POPCNT Instruction", 0x00000001, 0, REG_ECX, 23 },
    { GROUP_INSTRUCTION_SET, "PREFETCHI Instruction", 0x00000007, 1, REG_EDX, 14 },
    { GROUP_INSTRUCTION_SET, "PREFETCHWT1 Instruction", 0x00000007, 0, REG_ECX, 0 },
    { GROUP_INSTRUCTION_SET, "RDFSBASE / RDGSBASE / WRFSBASE / WRGSBASE Instruction",
      0x00000007, 0, REG_EBX, 0 },
    { GROUP_INSTRUCTION_SET, "RDPID Instruction", 0x00000007, 0, REG_ECX, 22 },
    { GROUP_INSTRUCTION_SET, "RDPRU Instruction", 0x80000008, 0, REG_EBX, 4 },
    { GROUP_INSTRUCTION_SET, "RDRAND Instruction", 0x00000001, 0, REG_ECX, 30 },
    { GROUP_INSTRUCTION_SET, "RDSEED Instruction", 0x00000007, 0, REG_EBX, 18 },
    { GROUP_INSTRUCTION_SET, "RDTSCP Instruction", 0x80000001, 0, REG_EDX, 27 },
    { GROUP_INSTRUCTION_SET, "SERIALIZE Instruction", 0x00000007, 0, REG_EDX, 14 },
    { GROUP_INSTRUCTION_SET, "SHA Extensions (SHA)", 0x00000007, 0, REG_EBX, 29 },
    { GROUP_INSTRUCTION_SET, "SKINIT / STGI Instruction", 0x80000001, 0, REG_ECX, 12 },
    { GROUP_INSTRUCTION_SET, "Streaming SIMD Extension (SSE)", 0x00000001, 0, REG_EDX, 25 },
    { GROUP_INSTRUCTION_SET, "Streaming SIMD Extension 2 (SSE2)", 0x00000001, 0, REG_EDX, 26 },
    { GROUP_INSTRUCTION_SET, "Streaming SIMD Extension 3 (SSE3)", 0x00000001, 0, REG_ECX, 0 },
    { GROUP_INSTRUCTION_SET, "Streaming SIMD Extension 4.1 (SSE4.1)", 0x00000001, 0, REG_ECX, 19 },
    { GROUP_INSTRUCTION_SET, "Streaming SIMD Extension 4.2 (SSE4.2)", 0x00000001, 0, REG_ECX, 20 },
    { GROUP_INSTRUCTION_SET, "Supplemental Streaming SIMD Extension 3 (SSSE3)",
      0x00000001, 0, REG_ECX, 9 },
    { GROUP_INSTRUCTION_SET, "SYSCALL / SYSRET Instruction", 0x80000001, 0, REG_EDX, 11 },
    { GROUP_INSTRUCTION_SET, "TPAUSE / UMONITOR / UMWAIT Instruction (WAITPKG)",
      0x00000007, 0, REG_ECX, 5 },
    { GROUP_INSTRUCTION_SET, "Trailing Bit Manipulation Instructions (TBM)",
      0x80000001, 0, REG_ECX, 21 },
    { GROUP_INSTRUCTION_SET, "WBNOINVD Instruction", 0x80000008, 0, REG_EBX, 9 },
    { GROUP_INSTRUCTION_SET, "WRMSRNS Instruction", 0x00000007, 1, REG_EAX, 19 },

    //----------------------------------------------------------------------------------------------
    // Security features.
    //----------------------------------------------------------------------------------------------
    { GROUP_SECURITY, "Control-flow Enforcement Indirect Branch Tracking (CET_IBT)",
      0x00000007, 0, REG_EDX, 20 },
    { GROUP_SECURITY, "Control-flow Enforcement Shadow Stack (CET_SS)",
      0x00000007, 0, REG_ECX, 7 },
    { GROUP_SECURITY, "Execution Disable Bit (NX, XD)", 0x80000001, 0, REG_EDX, 20 },
    { GROUP_SECURITY, "Indirect Branch Prediction Barrier (IBPB)", 0x80000008, 0, REG_EBX, 12 },
    { GROUP_SECURITY, "Indirect Branch Restricted Speculation (IBRS)",
      0x00000007, 0, REG_EDX, 26 },
    { GROUP_SECURITY, "Key Locker (KL)", 0x00000007, 0, REG_ECX, 23 },
    { GROUP_SECURITY, "L1 Data Cache Flush (L1D_FLUSH)", 0x00000007, 0, REG_EDX, 28 },
    { GROUP_SECURITY, "Linear Address Masking (LAM)", 0x00000007, 1, REG_EAX, 26 },
    { GROUP_SECURITY, "Linear Address Space Separation (LASS)", 0x00000007, 1, REG_EAX, 6 },
    { GROUP_SECURITY, "Memory Protection Extensions (MPX)", 0x00000007, 0, REG_EBX, 14 },
    { GROUP_SECURITY, "Memory Protection Keys for Supervisor-mode Pages (PKS)",
      0x00000007, 0, REG_ECX, 31 },
    { GROUP_SECURITY, "Memory Protection Keys for User-mode Pages (PKU)",
      0x00000007, 0, REG_ECX, 3 },
    { GROUP_SECURITY, "PKU Enabled by OS (OSPKE)", 0x00000007, 0, REG_ECX, 4 },
    { GROUP_SECURITY, "Predictive Store Forwarding Disable (PSFD)", 0x80000008, 0, REG_EBX, 28 },
    { GROUP_SECURITY, "Processor Serial Number (PSN)", 0x00000001, 0, REG_EDX, 18 },
    { GROUP_SECURITY, "Safe Mode Extensions (SMX)", 0x00000001, 0, REG_ECX, 6 },
    { GROUP_SECURITY, "SGX Launch Configuration", 0x00000007, 0, REG_ECX, 30 },
    { GROUP_SECURITY, "Single Thread Indirect Branch Predictors (STIBP)",
      0x00000007, 0, REG_EDX, 27 },
    { GROUP_SECURITY, "Software Guard Extensions (SGX)", 0x00000007, 0, REG_EBX, 2 },
    { GROUP_SECURITY, "Speculative Store Bypass Disable (SSBD)", 0x00000007, 0, REG_EDX, 31 },
    { GROUP_SECURITY, "Supervisor Mode Access Prevention (SMAP)", 0x00000007, 0, REG_EBX, 20 },
    { GROUP_SECURITY, "Supervisor-Mode Execution Prevention (SMEP)", 0x00000007, 0, REG_EBX, 7 },
    { GROUP_SECURITY, "Total Memory Encryption (TME)", 0x00000007, 0, REG_ECX, 13 },
    { GROUP_SECURITY, "User-mode Instruction Prevention (UMIP)", 0x00000007, 0, REG_ECX, 2 },
    { GROUP_SECURITY, "VERW Buffer Overwrite (MD_CLEAR)", 0x00000007, 0, REG_EDX, 10 },

    //----------------------------------------------------------------------------------------------
    // Power management features.
    //----------------------------------------------------------------------------------------------
    { GROUP_POWER, "100 MHz Multiplier Control", 0x80000007, 0, REG_EDX, 6 },
    { GROUP_POWER, "Always Running APIC Timer (ARAT)", 0x00000006, 0, REG_EAX, 2 },
    { GROUP_POWER, "Clock Modulation Duty Cycle Extension (ECMD)", 0x00000006, 0, REG_EAX, 5 },
    { GROUP_POWER, "Collaborative Processor Performance Control (CPPC)",
      0x80000008, 0, REG_EBX, 27 },
    { GROUP_POWER, "Core Performance Boost (CPB)", 0x80000007, 0, REG_EDX, 9 },
    { GROUP_POWER, "Digital Thermal Sensor (DTS)", 0x00000006, 0, REG_EAX, 0 },
    { GROUP_POWER, "Enhanced SpeedStep Technology (EIST, ESS)", 0x00000001, 0, REG_ECX, 7 },
    { GROUP_POWER, "Frequency Identification Control (FID)", 0x80000007, 0, REG_EDX, 1 },
    { GROUP_POWER, "Hardware Duty Cycling (HDC)", 0x00000006, 0, REG_EAX, 13 },
    { GROUP_POWER, "Hardware Feedback Interface", 0x00000006, 0, REG_EAX, 19 },
    { GROUP_POWER, "Hardware P-State Control (HwPstate)", 0x80000007, 0, REG_EDX, 7 },
    { GROUP_POWER, "Hardware P-States (HWP)", 0x00000006, 0, REG_EAX, 7 },
    { GROUP_POWER, "HWP Activity Window", 0x00000006, 0, REG_EAX, 9 },
    { GROUP_POWER, "HWP Capabilities", 0x00000006, 0, REG_EAX, 15 },
    { GROUP_POWER, "HWP Energy Performance Preference", 0x00000006, 0, REG_EAX, 10 },
    { GROUP_POWER, "HWP Notification", 0x00000006, 0, REG_EAX, 8 },
    { GROUP_POWER, "HWP Package Level Request", 0x00000006, 0, REG_EAX, 11 },
    { GROUP_POWER, "Invariant Time Stamp Counter (TscInvariant)", 0x80000007, 0, REG_EDX, 8 },
    { GROUP_POWER, "Package Thermal Management (PTM)", 0x00000006, 0, REG_EAX, 6 },
    { GROUP_POWER, "Power Limit Notification (PLN)", 0x00000006, 0, REG_EAX, 4 },
    { GROUP_POWER, "Processor Feedback Interface", 0x80000007, 0, REG_EDX, 11 },
    { GROUP_POWER, "Processor Power Reporting", 0x80000007, 0, REG_EDX, 12 },
    { GROUP_POWER, "Read-only Effective Frequency Interface (EffFreqRO)",
      0x80000007, 0, REG_EDX, 10 },
    { GROUP_POWER, "Temperature Sensor (TS)", 0x80000007, 0, REG_EDX, 0 },
    { GROUP_POWER, "Thermal Monitor (TM)", 0x00000001, 0, REG_EDX, 29 },
    { GROUP_POWER, "Thermal Monitor 2 (TM2)", 0x00000001, 0, REG_ECX, 8 },
    { GROUP_POWER, "Thermal Trip (TTP)", 0x80000007, 0, REG_EDX, 3 },
    { GROUP_POWER, "Thread Director", 0x00000006, 0, REG_EAX, 23 },
    { GROUP_POWER, "Turbo Boost Max Technology 3.0", 0x00000006, 0, REG_EAX, 14 },
    { GROUP_POWER, "Turbo Boost Technology", 0x00000006, 0, REG_EAX, 1 },
    { GROUP_POWER, "Voltage Identification Control (VID)", 0x80000007, 0, REG_EDX, 2 },

    //----------------------------------------------------------------------------------------------
    // Virtualization features.
    //----------------------------------------------------------------------------------------------
    { GROUP_VIRTUALIZATION, "Hypervisor", 0x00000001, 0, REG_ECX, 31 },
    { GROUP_VIRTUALIZATION, "Secure Virtual Machine (SVM, Pacifica)", 0x80000001, 0, REG_ECX, 2 },
    { GROUP_VIRTUALIZATION, "Virtual Machine Extensions (VMX, Vanderpool)",
      0x00000001, 0, REG_ECX, 5 },

    //----------------------------------------------------------------------------------------------
    // The rest of what the leaves report.
    //----------------------------------------------------------------------------------------------
    { GROUP_CPUID, "1 GB Page Size", 0x80000001, 0, REG_EDX, 26 },
    { GROUP_CPUID, "36-bit Page Size Extension (PSE36)", 0x00000001, 0, REG_EDX, 17 },
    { GROUP_CPUID, "5-Level Paging (LA57)", 0x00000007, 0, REG_ECX, 16 },
    { GROUP_CPUID, "64-Bit Debug Store (DTES64)", 0x00000001, 0, REG_ECX, 2 },
    { GROUP_CPUID, "Architectural Last Branch Records (LBR)", 0x00000007, 0, REG_EDX, 19 },
    { GROUP_CPUID, "Architectural Performance Monitoring Extended",
      0x00000007, 1, REG_EAX, 8 },
    { GROUP_CPUID, "Bus Lock Debug Exception", 0x00000007, 0, REG_ECX, 24 },
    { GROUP_CPUID, "Core Performance Counters", 0x80000001, 0, REG_ECX, 23 },
    { GROUP_CPUID, "CPL Qualified Debug Store", 0x00000001, 0, REG_ECX, 4 },
    { GROUP_CPUID, "Data Breakpoint Extension", 0x80000001, 0, REG_ECX, 26 },
    { GROUP_CPUID, "Debug Extension (DE)", 0x00000001, 0, REG_EDX, 2 },
    { GROUP_CPUID, "Debug Store (DS)", 0x00000001, 0, REG_EDX, 21 },
    { GROUP_CPUID, "Deprecated FPU CS and FPU DS", 0x00000007, 0, REG_EBX, 13 },
    { GROUP_CPUID, "Direct Cache Access (DCA)", 0x00000001, 0, REG_ECX, 18 },
    { GROUP_CPUID, "Extended APIC Register Space", 0x80000001, 0, REG_ECX, 3 },
    { GROUP_CPUID, "Extended Feature Disable (XFD)", 0x0000000D, 1, REG_EAX, 4 },
    { GROUP_CPUID, "Extended xAPIC Support (x2APIC)", 0x00000001, 0, REG_ECX, 21 },
    { GROUP_CPUID, "Fast System Call (SEP)", 0x00000001, 0, REG_EDX, 11 },
    { GROUP_CPUID, "Flexible Return and Event Delivery (FRED)", 0x00000007, 1, REG_EAX, 17 },
    { GROUP_CPUID, "Hybrid Processor", 0x00000007, 0, REG_EDX, 15 },
    { GROUP_CPUID, "Hyper-Threading Technology (HTT)", 0x00000001, 0, REG_EDX, 28 },
    { GROUP_CPUID, "IA32_ARCH_CAPABILITIES MSR", 0x00000007, 0, REG_EDX, 29 },
    { GROUP_CPUID, "IA32_CORE_CAPABILITIES MSR", 0x00000007, 0, REG_EDX, 30 },
    { GROUP_CPUID, "Instruction Based Sampling", 0x80000001, 0, REG_ECX, 10 },
    { GROUP_CPUID, "Instructions Retired Counter", 0x80000008, 0, REG_EBX, 1 },
    { GROUP_CPUID, "Intel Processor Trace (PT)", 0x00000007, 0, REG_EBX, 25 },
    { GROUP_CPUID, "L1 Context ID", 0x00000001, 0, REG_ECX, 10 },
    { GROUP_CPUID, "Last Level Cache Performance Counters", 0x80000001, 0, REG_ECX, 28 },
    { GROUP_CPUID, "Light Weight Profiling", 0x80000001, 0, REG_ECX, 15 },
    { GROUP_CPUID, "Machine-Check Architecture (MCA)", 0x00000001, 0, REG_EDX, 14 },
    { GROUP_CPUID, "Machine-Check Exception (MCE)", 0x00000001, 0, REG_EDX, 7 },
    { GROUP_CPUID, "Memory Type Range Registers (MTRR)", 0x00000001, 0, REG_EDX, 12 },
    { GROUP_CPUID, "Model Specific Registers (MSR)", 0x00000001, 0, REG_EDX, 5 },
    { GROUP_CPUID, "NB Performance Counters", 0x80000001, 0, REG_ECX, 24 },
    { GROUP_CPUID, "On-chip APIC Hardware (APIC)", 0x00000001, 0, REG_EDX, 9 },
    { GROUP_CPUID, "OS-Enabled Extended State Management (OSXSAVE)",
      0x00000001, 0, REG_ECX, 27 },
    { GROUP_CPUID, "Page Attribute Table (PAT)", 0x00000001, 0, REG_EDX, 16 },
    { GROUP_CPUID, "Page Global Enable (PGE)", 0x00000001, 0, REG_EDX, 13 },
    { GROUP_CPUID, "Page Size Extension (PSE)", 0x00000001, 0, REG_EDX, 3 },
    { GROUP_CPUID, "Pending Break Enable (PBE)", 0x00000001, 0, REG_EDX, 31 },
    { GROUP_CPUID, "Perfmon and Debug Capability", 0x00000001, 0, REG_ECX, 15 },
    { GROUP_CPUID, "Performance Time Stamp Counter (PTSC)", 0x80000001, 0, REG_ECX, 27 },
    { GROUP_CPUID, "Physical Address Extension (PAE)", 0x00000001, 0, REG_EDX, 6 },
    { GROUP_CPUID, "Platform Configuration (PCONFIG)", 0x00000007, 0, REG_EDX, 18 },
    { GROUP_CPUID, "Platform Quality of Service Enforcement (PQE)", 0x00000007, 0, REG_EBX, 15 },
    { GROUP_CPUID, "Platform Quality of Service Monitoring (PQM)", 0x00000007, 0, REG_EBX, 12 },
    { GROUP_CPUID, "Process Context Identifiers (PCID)", 0x00000001, 0, REG_ECX, 17 },
    { GROUP_CPUID, "Restore Error Pointers on XRSTOR", 0x80000008, 0, REG_EBX, 2 },
    { GROUP_CPUID, "Self-Snoop (SS)", 0x00000001, 0, REG_EDX, 27 },
    { GROUP_CPUID, "Silicon Debug Interface", 0x00000001, 0, REG_ECX, 11 },
    { GROUP_CPUID, "Thermal Monitor and Software Controlled Clock Facilities (ACPI)",
      0x00000001, 0, REG_EDX, 22 },
    { GROUP_CPUID, "Time Stamp Counter (TSC)", 0x00000001, 0, REG_EDX, 4 },
    { GROUP_CPUID, "Time Stamp Counter Deadline", 0x00000001, 0, REG_ECX, 24 },
    { GROUP_CPUID, "Topology Extensions", 0x80000001, 0, REG_ECX, 22 },
    { GROUP_CPUID, "Transactional Synchronization Extensions (HLE)",
      0x00000007, 0, REG_EBX, 4 },
    { GROUP_CPUID, "Transactional Synchronization Extensions (RTM)",
      0x00000007, 0, REG_EBX, 11 },
    { GROUP_CPUID, "TSX Suspend Load Address Tracking (TSXLDTRK)", 0x00000007, 0, REG_EDX, 16 },
    { GROUP_CPUID, "User Interrupts (UINTR)", 0x00000007, 0, REG_EDX, 5 },
    { GROUP_CPUID, "Virtual Mode Extension (VME)", 0x00000001, 0, REG_EDX, 1 },
    { GROUP_CPUID, "Watchdog Timer", 0x80000001, 0, REG_ECX, 13 },
    { GROUP_CPUID, "XGETBV with ECX = 1", 0x0000000D, 1, REG_EAX, 2 },
    { GROUP_CPUID, "xTPR Update Control", 0x00000001, 0, REG_ECX, 14 },
    { GROUP_CPUID, "XSAVE / XSTOR States", 0x00000001, 0, REG_ECX, 26 },
    { GROUP_CPUID, "XSAVEC and Compacted XRSTOR", 0x0000000D, 1, REG_EAX, 1 },
    { GROUP_CPUID, "XSAVEOPT Instruction", 0x0000000D, 1, REG_EAX, 0 },
    { GROUP_CPUID, "XSAVES / XRSTORS Instructions", 0x0000000D, 1, REG_EAX, 3 }
};

//--------------------------------------------------------------------------------------------------
// The leaves of the report, addressed the way the table above names them.
class Leafs
{
public:
    explicit Leafs(const proto::system_info::Processor& cpu)
    {
        for (int i = 0; i < cpu.cpuid_size(); ++i)
        {
            const proto::system_info::Processor::Cpuid& cpuid = cpu.cpuid(i);
            leafs_.insert(key(cpuid.leaf(), cpuid.subleaf()), &cpuid);
        }
    }

    bool contains(quint32 leaf, quint32 subleaf = 0) const
    {
        return leafs_.contains(key(leaf, subleaf));
    }

    quint32 value(quint32 leaf, quint32 subleaf, Register reg) const
    {
        const proto::system_info::Processor::Cpuid* cpuid = leafs_.value(key(leaf, subleaf), nullptr);
        if (!cpuid)
            return 0;

        switch (reg)
        {
            case REG_EAX: return cpuid->eax();
            case REG_EBX: return cpuid->ebx();
            case REG_ECX: return cpuid->ecx();
            default:      return cpuid->edx();
        }
    }

    bool bit(const FeatureBit& feature) const
    {
        const quint32 value_of_register =
            value(feature.leaf, feature.subleaf, feature.reg);
        return (value_of_register & (1u << feature.bit)) != 0;
    }

    // A string a leaf returns in its registers, four characters per register.
    QString string(quint32 leaf, std::initializer_list<Register> registers) const
    {
        QByteArray buffer;

        for (Register reg : registers)
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

    QHash<quint64, const proto::system_info::Processor::Cpuid*> leafs_;
};

class Item : public QTreeWidgetItem
{
public:
    Item(const QString& icon_path, const QString& text, const QList<QTreeWidgetItem*>& childs)
    {
        QIcon icon(icon_path);

        setIcon(0, icon);
        setText(0, text);

        for (const auto& child : childs)
        {
            child->setIcon(0, icon);

            for (int i = 0; i < child->childCount(); ++i)
                child->child(i)->setIcon(0, icon);
        }

        addChildren(childs);
    }

    // Group of parameters inside a group.
    Item(const QString& text, const QList<QTreeWidgetItem*>& params)
    {
        setText(0, text);
        addChildren(params);
    }

private:
    Q_DISABLE_COPY_MOVE(Item)
};

//--------------------------------------------------------------------------------------------------
QTreeWidgetItem* mk(const QString& param, const QString& value)
{
    QTreeWidgetItem* item = new QTreeWidgetItem();

    item->setText(0, param);
    item->setText(1, value);

    return item;
}

//--------------------------------------------------------------------------------------------------
// Values are shown as uppercase hex with a trailing 'h', the way the vendors document the leaves.
QString hex(quint32 value, int digits)
{
    return QString("%1h").arg(QString("%1").arg(value, digits, 16, QChar('0')).toUpper());
}

} // namespace

//--------------------------------------------------------------------------------------------------
SysInfoWidgetCpu::SysInfoWidgetCpu(QWidget* parent)
    : SysInfoWidget(parent),
      ui(std::make_unique<Ui::SysInfoCpu>())
{
    ui->setupUi(this);

    connect(ui->action_copy_row, &QAction::triggered, this, [this]()
    {
        copyRow(ui->tree->currentItem());
    });

    connect(ui->action_copy_name, &QAction::triggered, this, [this]()
    {
        copyColumn(ui->tree->currentItem(), 0);
    });

    connect(ui->action_copy_value, &QAction::triggered, this, [this]()
    {
        copyColumn(ui->tree->currentItem(), 1);
    });

    connect(ui->tree, &QTreeWidget::customContextMenuRequested,
            this, &SysInfoWidgetCpu::onContextMenu);

    connect(ui->tree, &QTreeWidget::itemDoubleClicked,
            this, [this](QTreeWidgetItem* item, int /* column */)
    {
        copyRow(item);
    });
}

//--------------------------------------------------------------------------------------------------
SysInfoWidgetCpu::~SysInfoWidgetCpu() = default;

//--------------------------------------------------------------------------------------------------
std::string SysInfoWidgetCpu::category() const
{
    // The processor arrives with the summary: the page shows the same report, only all of it.
    return kSystemInfo_Summary;
}

//--------------------------------------------------------------------------------------------------
void SysInfoWidgetCpu::setSystemInfo(const proto::system_info::SystemInfo& system_info)
{
    ui->tree->clear();

    // A report arrives without the processor when it was not asked about it. The next report may
    // still bring it.
    const bool has_processor = system_info.has_processor();

    ui->tree->setEnabled(has_processor);

    if (!has_processor)
        return;

    const proto::system_info::Processor& cpu = system_info.processor();

    QList<QTreeWidgetItem*> groups;

    const QList<QTreeWidgetItem*> properties = cpuidProperties(cpu);
    if (!properties.isEmpty())
        groups << new Item(kCpuIcon, tr("CPUID Properties"), properties);

    const QList<QTreeWidgetItem*> caches = cacheParameters(cpu);
    if (!caches.isEmpty())
        groups << new Item(kCpuIcon, tr("Caches"), caches);

    // A group is only shown when the leaves it is built from are there at all.
    const QString group_names[GROUP_COUNT] =
    {
        tr("Instruction Set"),
        tr("Security Features"),
        tr("Power Management Features"),
        tr("Virtualization Features"),
        tr("CPUID Features")
    };

    for (int group = 0; group < GROUP_COUNT; ++group)
    {
        const QList<QTreeWidgetItem*> features = featureParameters(cpu, group);
        if (features.isEmpty())
            continue;

        groups << new Item(kCpuIcon, group_names[group], features);
    }

    ui->tree->addTopLevelItems(groups);

    // The identification of the processor is what the category is opened for. The lists of the
    // features are long and stay collapsed until they are asked for.
    for (int i = 0; i < ui->tree->topLevelItemCount() && i < 2; ++i)
        ui->tree->expandItem(ui->tree->topLevelItem(i));

    if (!isStateRestored())
    {
        for (int i = 0; i < ui->tree->columnCount(); ++i)
            ui->tree->resizeColumnToContents(i);
    }
}

//--------------------------------------------------------------------------------------------------
QTreeWidget* SysInfoWidgetCpu::treeWidget()
{
    return ui->tree;
}

//--------------------------------------------------------------------------------------------------
void SysInfoWidgetCpu::onContextMenu(const QPoint& point)
{
    QTreeWidgetItem* current_item = ui->tree->itemAt(point);
    if (!current_item)
        return;

    ui->tree->setCurrentItem(current_item);

    QMenu menu;
    menu.addAction(ui->action_copy_row);
    menu.addAction(ui->action_copy_name);
    menu.addAction(ui->action_copy_value);

    menu.exec(ui->tree->viewport()->mapToGlobal(point));
}

//--------------------------------------------------------------------------------------------------
QList<QTreeWidgetItem*> SysInfoWidgetCpu::cpuidProperties(
    const proto::system_info::Processor& cpu) const
{
    const Leafs leafs(cpu);

    QList<QTreeWidgetItem*> items;

    const QString vendor = leafs.string(0x00000000, { REG_EBX, REG_EDX, REG_ECX });
    if (!vendor.isEmpty())
        items << mk(tr("CPUID Vendor"), vendor);

    // The brand string is spread over three leaves, four registers each.
    QString brand;
    for (quint32 leaf = 0x80000002; leaf <= 0x80000004; ++leaf)
        brand += leafs.string(leaf, { REG_EAX, REG_EBX, REG_ECX, REG_EDX });

    if (!brand.isEmpty())
        items << mk(tr("CPUID CPU Name"), brand.simplified());

    if (leafs.contains(0x00000001))
    {
        const quint32 signature = leafs.value(0x00000001, 0, REG_EAX);

        const quint32 base_family = (signature >> 8) & 0xF;
        const quint32 base_model = (signature >> 4) & 0xF;
        const quint32 extended_family = (signature >> 20) & 0xFF;
        const quint32 extended_model = (signature >> 16) & 0xF;

        items << mk(tr("CPUID Version"), hex(signature, 8));

        if (leafs.contains(0x80000001))
            items << mk(tr("Extended CPUID Version"), hex(leafs.value(0x80000001, 0, REG_EAX), 8));

        // The extended fields are only added to the ones they extend, and only the values that say
        // the fields are in use call for them.
        quint32 family = base_family;
        quint32 model = base_model;

        if (base_family == 0xF)
            family += extended_family;

        if (base_family == 0x6 || base_family == 0xF)
            model += extended_model << 4;

        items << mk(tr("Family"), hex(family, 2));
        items << mk(tr("Model"), hex(model, 2));
        items << mk(tr("Stepping"), hex(signature & 0xF, 2));

        if (extended_family)
            items << mk(tr("Extended Family"), hex(extended_family, 2));

        if (extended_model)
            items << mk(tr("Extended Model"), hex(extended_model, 2));

        items << mk(tr("Brand ID"), hex(leafs.value(0x00000001, 0, REG_EBX) & 0xFF, 2));
    }

    // The frequencies are only reported by a processor that supports the leaf.
    if (leafs.contains(0x00000016))
    {
        const quint32 base = leafs.value(0x00000016, 0, REG_EAX) & 0xFFFF;
        const quint32 maximum = leafs.value(0x00000016, 0, REG_EBX) & 0xFFFF;
        const quint32 bus = leafs.value(0x00000016, 0, REG_ECX) & 0xFFFF;

        if (base)
            items << mk(tr("Base Frequency"), tr("%1 MHz").arg(base));

        if (maximum)
            items << mk(tr("Max Frequency"), tr("%1 MHz").arg(maximum));

        if (bus)
            items << mk(tr("Bus Frequency"), tr("%1 MHz").arg(bus));
    }

    // The leaf is only defined when the processor runs under a hypervisor.
    const QString hypervisor = leafs.string(0x40000000, { REG_EBX, REG_ECX, REG_EDX });
    if (!hypervisor.isEmpty())
        items << mk(tr("Hypervisor"), hypervisor);

    if (cpu.packages())
        items << mk(tr("Packages"), QString::number(cpu.packages()));

    if (cpu.cores())
        items << mk(tr("Physical Cores"), QString::number(cpu.cores()));

    if (cpu.threads())
        items << mk(tr("Logical Cores"), QString::number(cpu.threads()));

    return items;
}

//--------------------------------------------------------------------------------------------------
QList<QTreeWidgetItem*> SysInfoWidgetCpu::cacheParameters(
    const proto::system_info::Processor& cpu) const
{
    const Leafs leafs(cpu);

    QList<QTreeWidgetItem*> items;

    // Both vendors describe the caches the same way, Intel under a standard leaf and AMD under an
    // extended one.
    quint32 cache_leaf = 0;

    if (leafs.value(0x00000004, 0, REG_EAX) & 0x1F)
        cache_leaf = 0x00000004;
    else if (leafs.value(0x8000001D, 0, REG_EAX) & 0x1F)
        cache_leaf = 0x8000001D;

    if (!cache_leaf)
        return items;

    for (quint32 subleaf = 0; leafs.contains(cache_leaf, subleaf); ++subleaf)
    {
        const quint32 eax = leafs.value(cache_leaf, subleaf, REG_EAX);
        const quint32 ebx = leafs.value(cache_leaf, subleaf, REG_EBX);

        const quint32 type = eax & 0x1F;
        if (!type)
            break;

        const quint32 level = (eax >> 5) & 0x7;
        const quint32 ways = ((ebx >> 22) & 0x3FF) + 1;
        const quint32 partitions = ((ebx >> 12) & 0x3FF) + 1;
        const quint32 line_size = (ebx & 0xFFF) + 1;
        const quint32 sets = leafs.value(cache_leaf, subleaf, REG_ECX) + 1;

        QString title;

        switch (type)
        {
            case 1:
                title = tr("L%1 Data Cache").arg(level);
                break;

            case 2:
                title = tr("L%1 Instruction Cache").arg(level);
                break;

            default:
                title = tr("L%1 Cache").arg(level);
                break;
        }

        QList<QTreeWidgetItem*> params;

        params << mk(tr("Size"),
                     Formatter::sizeToString(quint64(ways) * partitions * line_size * sets));

        // A cache that holds every line in a single set reports all ones instead of a number of
        // ways.
        params << mk(tr("Associativity"), ((ebx >> 22) & 0x3FF) == 0x3FF ?
            tr("Fully associative") : tr("%1-way").arg(ways));

        params << mk(tr("Line Size"), tr("%1 bytes").arg(line_size));
        params << mk(tr("Sets"), QString::number(sets));
        params << mk(tr("Shared By"), tr("%1 threads").arg(((eax >> 14) & 0xFFF) + 1));

        items << new Item(title, params);
    }

    return items;
}

//--------------------------------------------------------------------------------------------------
QList<QTreeWidgetItem*> SysInfoWidgetCpu::featureParameters(
    const proto::system_info::Processor& cpu, int group) const
{
    const Leafs leafs(cpu);

    QList<const FeatureBit*> features;

    for (const FeatureBit& feature : kFeatures)
    {
        if (feature.group != group)
            continue;

        // A bit of a leaf the processor does not answer says nothing about the feature.
        if (!leafs.contains(feature.leaf, feature.subleaf))
            continue;

        features << &feature;
    }

    // A feature is looked up by its name, and the order the vendors put the bits in is not one.
    std::sort(features.begin(), features.end(),
              [](const FeatureBit* first, const FeatureBit* second)
    {
        return qstricmp(first->name, second->name) < 0;
    });

    QList<QTreeWidgetItem*> items;

    for (const FeatureBit* feature : std::as_const(features))
        items << mk(QString::fromLatin1(feature->name), leafs.bit(*feature) ? tr("Yes") : tr("No"));

    return items;
}
