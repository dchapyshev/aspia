//
// PROJECT:         Aspia
// FILE:            system_info/category_cpu.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "system_info/category_cpu.h"
#include "system_info/category_cpu.pb.h"
#include "base/strings/string_util.h"
#include "base/cpu_info.h"
#include "ui/resource.h"

namespace aspia {

CategoryCPU::CategoryCPU()
    : CategoryInfo(Type::INFO_PARAM_VALUE)
{
    // Nothing
}

const char* CategoryCPU::Name() const
{
    return "Central Processor";
}

Category::IconId CategoryCPU::Icon() const
{
    return IDI_PROCESSOR;
}

const char* CategoryCPU::Guid() const
{
    return "{31D1312E-85A9-419A-91B4-BA81129B3CCC}";
}

void CategoryCPU::Parse(Table& table, const std::string& data)
{
    proto::CPU message;

    if (!message.ParseFromString(data))
        return;

    table.AddColumns(ColumnList::Create()
                     .AddColumn("Parameter", 300)
                     .AddColumn("Value", 260));

    table.AddParam("Brand String", Value::String(message.brand_string()));
    table.AddParam("Vendor", Value::String(message.vendor()));
    table.AddParam("Stepping", Value::FormattedString("%02Xh", message.stepping()));
    table.AddParam("Model", Value::FormattedString("%02Xh", message.model()));
    table.AddParam("Family", Value::FormattedString("%02Xh", message.family()));

    if (message.extended_model())
    {
        table.AddParam("Extended Model", Value::FormattedString("%02Xh", message.extended_model()));
    }

    if (message.extended_family())
    {
        table.AddParam("Extended Family", Value::FormattedString("%02Xh", message.extended_family()));
    }

    table.AddParam("Brand ID", Value::FormattedString("%02Xh", message.brand_id()));
    table.AddParam("Packages", Value::Number(message.packages()));
    table.AddParam("Physical Cores", Value::Number(message.physical_cores()));
    table.AddParam("Logical Cores", Value::Number(message.logical_cores()));

    if (message.has_features())
    {
        std::vector<std::pair<std::string, bool>> list;

        auto add = [&](bool is_supported, const char* name)
        {
            list.emplace_back(name, is_supported);
        };

        auto add_group = [&](const char* name)
        {
            std::sort(list.begin(), list.end());

            Group group = table.AddGroup(name);

            for (const auto& list_item : list)
            {
                group.AddParam(list_item.first, Value::Bool(list_item.second));
            }

            list.clear();
        };

        const proto::CPU::Features& features = message.features();

        add(features.has_femms(), "VIA FEMMS Instruction");
        add(features.has_tbm(), "Trailing Bit Manipulation Instructions");
        add(features.has_mwaitx(), "MONITORX / MWAITX Instruction");
        add(features.has_skinit(), "SKINIT / STGI Instruction");
        add(features.has_syscall(), "SYSCALL / SYSRET Instruction");
        add(features.has_rdtscp(), "RDTSCP Instruction");
        add(features.has_rdseed(), "RDSEED Instruction");
        add(features.has_rdrand(), "RDRAND Instruction");
        add(features.has_fsgsbase(), "RDFSBASE / RDGSBASE / WRFSBASE / WRGSBASE Instruction");
        add(features.has_prefetchwt1(), "PREFETCHWT1 Instruction");
        add(features.has_popcnt(), "POPCNT Instruction");
        add(features.has_pcommit(), "PCOMMIT Instruction");
        add(features.has_movbe(), "MOVBE Instruction");
        add(features.has_monitor(), "MONITOR / MWAIT Instruction");
        add(features.has_lzcnt(), "LZCNT Instruction");
        add(features.has_lahf(), "LAHF / SAHF Instruction");
        add(features.has_invpcid(), "INVPCID Instruction");
        add(features.has_cmov(), "Conditional Move Instruction (CMOV)");
        add(features.has_cx16(), "CMPXCHG16B Instruction");
        add(features.has_cx8(), "CMPXCHG8B Instruction");
        add(features.has_clwb(), "CLWB Instruction");
        add(features.has_clflushopt(), "CLFLUSHOPT Instruction");
        add(features.has_clfsh(), "CLFLUSH Instruction");
        add(features.has_ais(), "VIA Alternate Instruction Set Supported");
        add(features.has_ais_e(), "VIA Alternate Instruction Set Enabled");
        add(features.has_fma(), "Fused Multiply Add (FMA)");
        add(features.has_aes(), "AES Instruction (AES)");
        add(features.has_erms(), "Enhanced REP MOVSB/STOSB");
        add(features.has_sse(), "Streaming SIMD Extension (SSE)");
        add(features.has_sse2(), "Streaming SIMD Extension 2 (SSE2)");
        add(features.has_sse3(), "Streaming SIMD Extension 3 (SSE3)");
        add(features.has_ssse3(), "Supplemental Streaming SIMD Extension 3 (SSSE3)");
        add(features.has_sse41(), "Streaming SIMD Extension 4.1 (SSE4.1)");
        add(features.has_sse42(), "Streaming SIMD Extension 4.2 (SSE4.2)");
        add(features.has_avx(), "Advanced Vector Extension (AVX)");
        add(features.has_avx2(), "Advanced Vector Extensions 2 (AVX2)");
        add(features.has_avx512f(), "AVX-512 Foundation (AVX512F)");
        add(features.has_avx512dq(), "AVX-512 Doubleword and Quadword Instructions (AVX512DQ)");
        add(features.has_avx512ifma(), "AVX-512 Integer Fused Multiply-Add Instructions (AVX512IFMA)");
        add(features.has_avx512pf(), "AVX-512 Prefetch Instructions (AVX512PF)");
        add(features.has_avx512er(), "AVX-512 Exponential and Reciprocal Instructions (AVX512ER)");
        add(features.has_avx512cd(), "AVX-512 Conflict Detection Instructions (AVX512CD)");
        add(features.has_avx512vbmi(), "AVX-512 Vector Bit Manipulation Instructions (AVX512VBMI)");
        add(features.has_avx512bw(), "AVX-512 Byte and Word Instructions (AVX512BW)");
        add(features.has_avx512vl(), "AVX-512 Vector Length Extensions (AVX512VL)");
        add(features.has_bmi1(), "Bit Manipulation Instruction Set 1 (BMI1)");
        add(features.has_bmi2(), "Bit Manipulation Instruction Set 2 (BMI2)");
        add(features.has_avx512vnni(), "AVX-512 Vector Neural Network Instructions (AVX512VNNI)");
        add(features.has_avx512bitalg(), "AVX-512 BITALG Instructions (AVX512BITALG)");
        add(features.has_avx512vpopcntdq(), "AVX-512 Vector Population Count D/Q (AVX512VPOPCNTDQ)");
        add(features.has_avx512vbmi2(), "AVX-512 Vector Bit Manipulation Instructions 2 (AVX512VBMI2)");
        add(features.has_avx512_4vnniw(), "AVX-512 4-register Neural Network Instructions (AVX5124VNNIW)");
        add(features.has_avx512_4fmaps(), "AVX-512 4-register Multiply Accumulation Single precision (AVX5124FMAPS)");
        add(features.has_f16c(), "Float-16-bit Convertsion Instructions (F16C)");
        add(features.has_sse4a(), "AMD SSE4A");
        add(features.has_misalignsse(), "AMD MisAligned SSE");
        add(features.has_3dnow_prefetch(), "AMD 3DNowPrefetch");
        add(features.has_mmxext(), "AMD Extended MMX");
        add(features.has_3dnowext(), "AMD Extended 3DNow!");
        add(features.has_3dnow(), "AMD 3DNow!");
        add(features.has_xop(), "AMD XOP");
        add(features.has_fma4(), "AMD FMA4");
        add(features.has_mmx(), "MMX Technology (MMX)");
        add(features.has_vaes(), "AES Instruction Set (VEX-256/EVEX)");
        add(features.has_vpclmulqdq(), "CLMUL Instruction Set (VEX-256/EVEX)");
        add(features.has_sha(), "Intel SHA Extensions (SHA)");

        add(features.has_fpu(), "Floating-point Unit On-Chip (FPU)");
        add(features.has_pclmuldq(), "PCLMULDQ Instruction");
        add(features.has_gfni(), "Galois Field Instructions (GFNI)");
        add(features.has_fxsr(), "FXSAVE / FXSTOR Instruction");
        add(features.has_intel64(), "64-bit x86 Extension (AMD64, Intel64)");

        add_group("Instruction Set");

        add(features.has_ace(), "Advanced Cryptography Engine (ACE) Supported");
        add(features.has_ace_e(), "Advanced Cryptography Engine (ACE) Enabled");
        add(features.has_ace2(), "Advanced Cryptography Engine 2 (ACE2) Supported");
        add(features.has_ace2_e(), "Advanced Cryptography Engine 2 (ACE2) Enabled");
        add(features.has_phe(), "PadLock Hash Engine (PHE) Supported");
        add(features.has_phe_e(), "PadLock Hash Engine (PHE) Enabled");
        add(features.has_pmm(), "PadLock Montgomery Multiplier (PMM) Supported");
        add(features.has_pmm_e(), "PadLock Montgomery Multiplier (PMM) Enabled");
        add(features.has_rng(), "Hardware Random Number Generator (RNG) Supported");
        add(features.has_rng_e(), "Hardware Random Number Generator (RNG) Enabled");
        add(features.has_rng2(), "Hardware Random Number Generator 2 (RNG2) Supported");
        add(features.has_rng2_e(), "Hardware Random Number Generator 2 (RNG2) Enabled");
        add(features.has_phe2(), "PadLock Hash Engine 2 (PHE2) Supported");
        add(features.has_phe2_e(), "PadLock Hash Engine 2 (PHE2) Enabled");
        add(features.has_smx(), "Safe Mode Extensions (SMX)");
        add(features.has_smep(), "Supervisor-Mode Execution Prevention (SMEP)");
        add(features.has_smap(), "Supervisor Mode Access Prevention (SMAP)");
        add(features.has_sgx(), "Software Guard Extensions (SGE)");
        add(features.has_psn(), "Processor Serial Number (PSN)");
        add(features.has_mpx(), "Memory Protection Extensions (MPX)");

        add_group("Security Features");

        add(features.has_tm(), "Thermal Monitor (TM)");
        add(features.has_tm2(), "Thermal Monitor 2 (TM2)");
        add(features.has_parallax(), "Parallax Supported");
        add(features.has_parallax_e(), "Parallax Enabled");
        add(features.has_overstress(), "Overstress Supported");
        add(features.has_overstress_e(), "Overstress Enabled");
        add(features.has_tm3(), "Thermal Monitor 3 (TM3) Supported");
        add(features.has_tm3_e(), "Thermal Monitor 3 (TM3) Enabled");
        add(features.has_longrun(), "LongRun");
        add(features.has_lrti(), "LongRun Table Interface");
        add(features.has_est(), "Enhanced SpeedStep Technology (EIST, ESS)");

        add_group("Power Management Features");

        add(features.has_svm(), "Secure Virtual Machine (SVM, Pacifica)");
        add(features.has_vmx(), "Virtual Machine Extensions (VMX, Vanderpool)");
        add(features.has_hypervisor(), "Hypervisor");

        add_group("Virtualization Features");

        add(features.has_1gb_pages(), "1 GB Page Size");
        add(features.has_pse36(), "36-bit Page Size Extension (PSE36)");
        add(features.has_vme(), "Virtual Mode Extension (VME)");
        add(features.has_wdt(), "Watchdog Timer");
        add(features.has_x2apic(), "Extended xAPIC Support (x2APIC)");
        add(features.has_ds_cpl(), "CPL Qualified Debug Store");
        add(features.has_xsave(), "XSAVE/XSTOR States");
        add(features.has_htt(), "Hyper-Threading Technology (HTT)");
        add(features.has_ibs(), "Instruction Based Sampling");
        add(features.has_cnxt_id(), "L1 Context ID");
        add(features.has_pae(), "Physical Address Extension (PAE)");
        add(features.has_msr(), "Model Specific Registers (MSR)");
        add(features.has_pat(), "Page Attribute Table (PAT)");
        add(features.has_ss(), "Self-Snoop (SS)");
        add(features.has_pse(), "Page Size Extension (PSE)");
        add(features.has_perfctr_nb(), "NB Performance Counters");
        add(features.has_perfctr_core(), "Core Performance Counters");
        add(features.has_lwp(), "Light Weight Profiling");
        add(features.has_intel_pt(), "Intel Processor Trace (PT)");
        add(features.has_tsc(), "Time Stamp Counter (TSC)");
        add(features.has_de(), "Debug Extension (DE)");
        add(features.has_ptsc(), "Performance Time Stamp Counter (PTSC)");
        add(features.has_extapic(), "Extended APIC Register Space");
        add(features.has_dca(), "Direct Cache Access (DCA)");
        add(features.has_mca(), "Machine-Check Architecture (MCA)");
        add(features.has_pqm(), "Platform Quality of Service Monitoring (PQM)");
        add(features.has_pqe(), "Platform Quality of Service Enforcement (PQE)");
        add(features.has_bpext(), "Data Breakpoint Extension");
        add(features.has_mce(), "Machine-Check Exception (MCE)");
        add(features.has_apic(), "On-cpip APIC Hardware (APIC)");
        add(features.has_pcid(), "Process Context Identifiers (PCID)");
        add(features.has_mtrr(), "Memory Type Range Registers (MTRR)");

        add(features.has_lh(), "LongHaul MSR 0000_110Ah");
        add(features.has_xtpr(), "xTPR Update Control");
        add(features.has_dtes64(), "64-Bit Debug Store (DTES64)");
        add(features.has_sdbg(), "Silicon Debug Interface");
        add(features.has_pdcm(), "Perfmon and Debug Capability");
        add(features.has_tsc_deadline(), "Time Stamp Counter Deadline");
        add(features.has_sep(), "Fast System Call (SEP)");
        add(features.has_pge(), "Page Global Enable (PGE)");
        add(features.has_ds(), "Debug Store (DS)");
        add(features.has_acpu(), "Thermal Monitor and Software Controlled Clock Facilities (ACPU)");
        add(features.has_ia64(), "IA64 Processor Emulating x86");
        add(features.has_pbe(), "Pending Break Enable (PBE)");
        add(features.has_osxsave(), "OS-Enabled Extended State Management");
        add(features.has_xd_bit(), "Execution Disable Bit");
        add(features.has_hle(), "Transactional Synchronization Extensions (HLE)");
        add(features.has_rtm(), "Transactional Synchronization Extensions (RTM)");
        add(features.has_adx(), "Multi-Precision Add-Carry Instruction Extensions (ADX)");
        add(features.has_umip(), "User-mode Instruction Prevention (UMIP)");
        add(features.has_pku(), "Memory Protection Keys for User-mode pages (PKU)");
        add(features.has_ospke(), "PKU enabled by OS (OSPKE)");
        add(features.has_rdpid(), "Read Processor ID (RDPID)");
        add(features.has_sgx_lc(), "SGX Launch Configuration");

        add_group("CPUID Features");
    }
}

std::string CategoryCPU::Serialize()
{
    proto::CPU message;

    CPUInfo cpu;
    GetCPUInformation(cpu);

    message.set_brand_string(CollapseWhitespaceASCII(cpu.brand_string, true));
    message.set_vendor(CollapseWhitespaceASCII(cpu.vendor, true));
    message.set_stepping(cpu.stepping);
    message.set_model(cpu.model);
    message.set_family(cpu.family);
    message.set_extended_model(cpu.extended_model);
    message.set_extended_family(cpu.extended_family);
    message.set_brand_id(cpu.brand_id);
    message.set_packages(cpu.package_count);
    message.set_physical_cores(cpu.physical_core_count);
    message.set_logical_cores(cpu.logical_core_count);

    proto::CPU::Features* features = message.mutable_features();

    // Function 1 EDX
    features->set_has_fpu(cpu.fn_1_edx.has_fpu);
    features->set_has_vme(cpu.fn_1_edx.has_vme);
    features->set_has_de(cpu.fn_1_edx.has_de);
    features->set_has_pse(cpu.fn_1_edx.has_pse);
    features->set_has_tsc(cpu.fn_1_edx.has_tsc);
    features->set_has_msr(cpu.fn_1_edx.has_msr);
    features->set_has_pae(cpu.fn_1_edx.has_pae);
    features->set_has_mce(cpu.fn_1_edx.has_mce);
    features->set_has_cx8(cpu.fn_1_edx.has_cx8);
    features->set_has_apic(cpu.fn_1_edx.has_apic);
    features->set_has_sep(cpu.fn_1_edx.has_sep);
    features->set_has_mtrr(cpu.fn_1_edx.has_mtrr);
    features->set_has_pge(cpu.fn_1_edx.has_pge);
    features->set_has_mca(cpu.fn_1_edx.has_mca);
    features->set_has_cmov(cpu.fn_1_edx.has_cmov);
    features->set_has_pat(cpu.fn_1_edx.has_pat);
    features->set_has_pse36(cpu.fn_1_edx.has_pse36);
    features->set_has_psn(cpu.fn_1_edx.has_psn);
    features->set_has_clfsh(cpu.fn_1_edx.has_clfsh);
    features->set_has_ds(cpu.fn_1_edx.has_ds);
    features->set_has_acpu(cpu.fn_1_edx.has_acpu);
    features->set_has_mmx(cpu.fn_1_edx.has_mmx);
    features->set_has_fxsr(cpu.fn_1_edx.has_fxsr);
    features->set_has_sse(cpu.fn_1_edx.has_sse);
    features->set_has_sse2(cpu.fn_1_edx.has_sse2);
    features->set_has_ss(cpu.fn_1_edx.has_ss);
    features->set_has_htt(cpu.fn_1_edx.has_htt);
    features->set_has_tm(cpu.fn_1_edx.has_tm);
    features->set_has_ia64(cpu.fn_1_edx.has_ia64);
    features->set_has_pbe(cpu.fn_1_edx.has_pbe);

    // Function 1 ECX
    features->set_has_sse3(cpu.fn_1_ecx.has_sse3);
    features->set_has_pclmuldq(cpu.fn_1_ecx.has_pclmuldq);
    features->set_has_dtes64(cpu.fn_1_ecx.has_dtes64);
    features->set_has_monitor(cpu.fn_1_ecx.has_monitor);
    features->set_has_ds_cpl(cpu.fn_1_ecx.has_ds_cpl);
    features->set_has_vmx(cpu.fn_1_ecx.has_vmx);
    features->set_has_smx(cpu.fn_1_ecx.has_smx);
    features->set_has_est(cpu.fn_1_ecx.has_est);
    features->set_has_tm2(cpu.fn_1_ecx.has_tm2);
    features->set_has_ssse3(cpu.fn_1_ecx.has_ssse3);
    features->set_has_cnxt_id(cpu.fn_1_ecx.has_cnxt_id);
    features->set_has_sdbg(cpu.fn_1_ecx.has_sdbg);
    features->set_has_fma(cpu.fn_1_ecx.has_fma);
    features->set_has_cx16(cpu.fn_1_ecx.has_cx16);
    features->set_has_xtpr(cpu.fn_1_ecx.has_xtpr);
    features->set_has_pdcm(cpu.fn_1_ecx.has_pdcm);
    features->set_has_pcid(cpu.fn_1_ecx.has_pcid);
    features->set_has_dca(cpu.fn_1_ecx.has_dca);
    features->set_has_sse41(cpu.fn_1_ecx.has_sse41);
    features->set_has_sse42(cpu.fn_1_ecx.has_sse42);
    features->set_has_x2apic(cpu.fn_1_ecx.has_x2apic);
    features->set_has_movbe(cpu.fn_1_ecx.has_movbe);
    features->set_has_popcnt(cpu.fn_1_ecx.has_popcnt);
    features->set_has_tsc_deadline(cpu.fn_1_ecx.has_tsc_deadline);
    features->set_has_aes(cpu.fn_1_ecx.has_aes);
    features->set_has_xsave(cpu.fn_1_ecx.has_xsave);
    features->set_has_osxsave(cpu.fn_1_ecx.has_osxsave);
    features->set_has_avx(cpu.fn_1_ecx.has_avx);
    features->set_has_f16c(cpu.fn_1_ecx.has_f16c);
    features->set_has_rdrand(cpu.fn_1_ecx.has_rdrand);
    features->set_has_hypervisor(cpu.fn_1_ecx.has_hypervisor);

    // Function 0x80000001 EDX
    features->set_has_syscall(cpu.fn_81_edx.has_syscall);
    features->set_has_xd_bit(cpu.fn_81_edx.has_xd_bit);
    features->set_has_mmxext(cpu.fn_81_edx.has_mmxext);
    features->set_has_1gb_pages(cpu.fn_81_edx.has_1gb_pages);
    features->set_has_rdtscp(cpu.fn_81_edx.has_rdtscp);
    features->set_has_intel64(cpu.fn_81_edx.has_intel64);
    features->set_has_3dnowext(cpu.fn_81_edx.has_3dnowext);
    features->set_has_3dnow(cpu.fn_81_edx.has_3dnow);

    // Function 0x80000001 ECX
    features->set_has_lahf(cpu.fn_81_ecx.has_lahf);
    features->set_has_svm(cpu.fn_81_ecx.has_svm);
    features->set_has_extapic(cpu.fn_81_ecx.has_extapic);
    features->set_has_lzcnt(cpu.fn_81_ecx.has_lzcnt);
    features->set_has_sse4a(cpu.fn_81_ecx.has_sse4a);
    features->set_has_misalignsse(cpu.fn_81_ecx.has_misalignsse);
    features->set_has_3dnow_prefetch(cpu.fn_81_ecx.has_3dnow_prefetch);
    features->set_has_ibs(cpu.fn_81_ecx.has_ibs);
    features->set_has_xop(cpu.fn_81_ecx.has_xop);
    features->set_has_skinit(cpu.fn_81_ecx.has_skinit);
    features->set_has_wdt(cpu.fn_81_ecx.has_wdt);
    features->set_has_lwp(cpu.fn_81_ecx.has_lwp);
    features->set_has_fma4(cpu.fn_81_ecx.has_fma4);
    features->set_has_tbm(cpu.fn_81_ecx.has_tbm);
    features->set_has_perfctr_core(cpu.fn_81_ecx.has_perfctr_core);
    features->set_has_perfctr_nb(cpu.fn_81_ecx.has_perfctr_nb);
    features->set_has_bpext(cpu.fn_81_ecx.has_bpext);
    features->set_has_ptsc(cpu.fn_81_ecx.has_ptsc);
    features->set_has_mwaitx(cpu.fn_81_ecx.has_mwaitx);

    // Function 7 EBX
    features->set_has_fsgsbase(cpu.fn_7_0_ebx.has_fsgsbase);
    features->set_has_sgx(cpu.fn_7_0_ebx.has_sgx);
    features->set_has_bmi1(cpu.fn_7_0_ebx.has_bmi1);
    features->set_has_hle(cpu.fn_7_0_ebx.has_hle);
    features->set_has_avx2(cpu.fn_7_0_ebx.has_avx2);
    features->set_has_smep(cpu.fn_7_0_ebx.has_smep);
    features->set_has_bmi2(cpu.fn_7_0_ebx.has_bmi2);
    features->set_has_erms(cpu.fn_7_0_ebx.has_erms);
    features->set_has_invpcid(cpu.fn_7_0_ebx.has_invpcid);
    features->set_has_rtm(cpu.fn_7_0_ebx.has_rtm);
    features->set_has_pqm(cpu.fn_7_0_ebx.has_pqm);
    features->set_has_mpx(cpu.fn_7_0_ebx.has_mpx);
    features->set_has_pqe(cpu.fn_7_0_ebx.has_pqe);
    features->set_has_avx512f(cpu.fn_7_0_ebx.has_avx512f);
    features->set_has_avx512dq(cpu.fn_7_0_ebx.has_avx512dq);
    features->set_has_rdseed(cpu.fn_7_0_ebx.has_rdseed);
    features->set_has_adx(cpu.fn_7_0_ebx.has_adx);
    features->set_has_smap(cpu.fn_7_0_ebx.has_smap);
    features->set_has_avx512ifma(cpu.fn_7_0_ebx.has_avx512ifma);
    features->set_has_pcommit(cpu.fn_7_0_ebx.has_pcommit);
    features->set_has_clflushopt(cpu.fn_7_0_ebx.has_clflushopt);
    features->set_has_clwb(cpu.fn_7_0_ebx.has_clwb);
    features->set_has_intel_pt(cpu.fn_7_0_ebx.has_intel_pt);
    features->set_has_avx512pf(cpu.fn_7_0_ebx.has_avx512pf);
    features->set_has_avx512er(cpu.fn_7_0_ebx.has_avx512er);
    features->set_has_avx512cd(cpu.fn_7_0_ebx.has_avx512cd);
    features->set_has_sha(cpu.fn_7_0_ebx.has_sha);
    features->set_has_avx512bw(cpu.fn_7_0_ebx.has_avx512bw);
    features->set_has_avx512vl(cpu.fn_7_0_ebx.has_avx512vl);

    // Function 7 ECX
    features->set_has_prefetchwt1(cpu.fn_7_0_ecx.has_prefetchwt1);
    features->set_has_avx512vbmi(cpu.fn_7_0_ecx.has_avx512vbmi);
    features->set_has_umip(cpu.fn_7_0_ecx.has_umip);
    features->set_has_pku(cpu.fn_7_0_ecx.has_pku);
    features->set_has_ospke(cpu.fn_7_0_ecx.has_ospke);
    features->set_has_avx512vbmi2(cpu.fn_7_0_ecx.has_avx512vbmi2);
    features->set_has_gfni(cpu.fn_7_0_ecx.has_gfni);
    features->set_has_vaes(cpu.fn_7_0_ecx.has_vaes);
    features->set_has_vpclmulqdq(cpu.fn_7_0_ecx.has_vpclmulqdq);
    features->set_has_avx512vnni(cpu.fn_7_0_ecx.has_avx512vnni);
    features->set_has_avx512bitalg(cpu.fn_7_0_ecx.has_avx512bitalg);
    features->set_has_avx512vpopcntdq(cpu.fn_7_0_ecx.has_avx512vpopcntdq);
    features->set_has_rdpid(cpu.fn_7_0_ecx.has_rdpid);
    features->set_has_sgx_lc(cpu.fn_7_0_ecx.has_sgx_lc);

    // Function 7 EDX
    features->set_has_avx512_4vnniw(cpu.fn_7_0_edx.has_avx512_4vnniw);
    features->set_has_avx512_4fmaps(cpu.fn_7_0_edx.has_avx512_4fmaps);

    // Function 0xC0000001 EDX
    features->set_has_ais(cpu.fn_c1_edx.has_ais);
    features->set_has_rng(cpu.fn_c1_edx.has_rng);
    features->set_has_lh(cpu.fn_c1_edx.has_lh);
    features->set_has_femms(cpu.fn_c1_edx.has_femms);
    features->set_has_ace(cpu.fn_c1_edx.has_ace);
    features->set_has_ace2(cpu.fn_c1_edx.has_ace2);
    features->set_has_phe(cpu.fn_c1_edx.has_phe);
    features->set_has_pmm(cpu.fn_c1_edx.has_pmm);
    features->set_has_parallax(cpu.fn_c1_edx.has_parallax);
    features->set_has_overstress(cpu.fn_c1_edx.has_overstress);
    features->set_has_tm3(cpu.fn_c1_edx.has_tm3);
    features->set_has_rng2(cpu.fn_c1_edx.has_rng2);
    features->set_has_phe2(cpu.fn_c1_edx.has_phe2);

    // Function 0x80860001 EDX
    features->set_has_longrun(cpu.fn_80860001_edx.has_longrun);
    features->set_has_lrti(cpu.fn_80860001_edx.has_lrti);

    return message.SerializeAsString();
}

} // namespace aspia
