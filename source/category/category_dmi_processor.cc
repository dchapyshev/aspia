//
// PROJECT:         Aspia
// FILE:            category/category_dmi_processor.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/devices/smbios_reader.h"
#include "base/strings/string_printf.h"
#include "base/bitset.h"
#include "category/category_dmi_processor.h"
#include "category/category_dmi_processor.pb.h"
#include "ui/resource.h"

namespace aspia {

namespace {

const char* FamilyToString(proto::DmiProcessor::Family value)
{
    switch (value)
    {
        case proto::DmiProcessor::FAMILY_OTHER:
            return "Other";

        case proto::DmiProcessor::FAMILY_8086:
            return "8086";

        case proto::DmiProcessor::FAMILY_80286:
            return "80286";

        case proto::DmiProcessor::FAMILY_INTEL_386_PROCESSOR:
            return "Intel386";

        case proto::DmiProcessor::FAMILY_INTEL_486_PROCESSOR:
            return "Intel486";

        case proto::DmiProcessor::FAMILY_8087:
            return "8087";

        case proto::DmiProcessor::FAMILY_80287:
            return "80287";

        case proto::DmiProcessor::FAMILY_80387:
            return "80387";

        case proto::DmiProcessor::FAMILY_80487:
            return "80487";

        case proto::DmiProcessor::FAMILY_INTEL_PENTIUM_PROCESSOR:
            return "Intel Pentium";

        case proto::DmiProcessor::FAMILY_INTEL_PENTIUM_PRO_PROCESSOR:
            return "Intel Pentium Pro";

        case proto::DmiProcessor::FAMILY_INTEL_PENTIUM_2_PROCESSOR:
            return "Intel Pentium II";

        case proto::DmiProcessor::FAMILY_PENTIUM_PROCESSOR_WITH_MMX:
            return "Intel Pentium with MMX Technology";

        case proto::DmiProcessor::FAMILY_INTEL_CELERON_PROCESSOR:
            return "Intel Celeron";

        case proto::DmiProcessor::FAMILY_INTEL_PENTIUM_2_XEON_PROCESSOR:
            return "Intel Pentium II Xeon";

        case proto::DmiProcessor::FAMILY_INTEL_PENTIUM_3_PROCESSOR:
            return "Intel Pentium III";

        case proto::DmiProcessor::FAMILY_M1_FAMILY:
            return "M1";

        case proto::DmiProcessor::FAMILY_M2_FAMILY:
            return "M2";

        case proto::DmiProcessor::FAMILY_INTEL_CELEROM_M_PROCESSOR:
            return "Intel Celeron M";

        case proto::DmiProcessor::FAMILY_INTEL_PENTIUM_4_HT_PROCESSOR:
            return "Intel Pentium 4 HT";

        case proto::DmiProcessor::FAMILY_AMD_DURON_PROCESSOR_FAMILY:
            return "AMD Duron";

        case proto::DmiProcessor::FAMILY_AMD_K5_FAMILY:
            return "AMD K5";

        case proto::DmiProcessor::FAMILY_AMD_K6_FAMILY:
            return "AMD K6";

        case proto::DmiProcessor::FAMILY_AMD_K6_2:
            return "AMD K6-2";

        case proto::DmiProcessor::FAMILY_AMD_K6_3:
            return "AMD K6-3";

        case proto::DmiProcessor::FAMILY_AMD_ATHLON_PROCESSOR_FAMILY:
            return "AMD Athlon";

        case proto::DmiProcessor::FAMILY_AMD_29000_FAMILY:
            return "AMD 29000";

        case proto::DmiProcessor::FAMILY_AMD_K6_2_PLUS:
            return "AMD K6-2+";

        case proto::DmiProcessor::FAMILY_POWER_PC_FAMILY:
            return "Power PC";

        case proto::DmiProcessor::FAMILY_POWER_PC_601:
            return "Power PC 601";

        case proto::DmiProcessor::FAMILY_POWER_PC_603:
            return "Power PC 603";

        case proto::DmiProcessor::FAMILY_POWER_PC_603_PLUS:
            return "Power PC 603+";

        case proto::DmiProcessor::FAMILY_POWER_PC_604:
            return "Power PC 604";

        case proto::DmiProcessor::FAMILY_POWER_PC_620:
            return "Power PC 620";

        case proto::DmiProcessor::FAMILY_POWER_PC_X704:
            return "Power PC x704";

        case proto::DmiProcessor::FAMILY_POWER_PC_750:
            return "Power PC 750";

        case proto::DmiProcessor::FAMILY_INTEL_CORE_DUO_PROCESSOR:
            return "Intel Core Duo";

        case proto::DmiProcessor::FAMILY_INTEL_CORE_DUO_MOBILE_PROCESSOR:
            return "Intel Core Duo Mobile";

        case proto::DmiProcessor::FAMILY_INTEL_CORE_SOLO_MOBILE_PROCESSOR:
            return "Intel Core Solo Mobile";

        case proto::DmiProcessor::FAMILY_INTEL_ATOM_PROCESSOR:
            return "Intel Atom";

        case proto::DmiProcessor::FAMILY_INTEL_CORE_M_PROCESSOR:
            return "Intel Core M";

        case proto::DmiProcessor::FAMILY_INTEL_CORE_M3_PROCESSOR:
            return "Intel Core m3";

        case proto::DmiProcessor::FAMILY_INTEL_CORE_M5_PROCESSOR:
            return "Intel Core m5";

        case proto::DmiProcessor::FAMILY_INTEL_CORE_M7_PROCESSOR:
            return "Intel Core m7";

        case proto::DmiProcessor::FAMILY_ALPHA_FAMILY:
            return "Alpha";

        case proto::DmiProcessor::FAMILY_ALPHA_21064:
            return "Alpha 21064";

        case proto::DmiProcessor::FAMILY_ALPHA_21066:
            return "Alpha 21066";

        case proto::DmiProcessor::FAMILY_ALPHA_21164:
            return "Alpha 21164";

        case proto::DmiProcessor::FAMILY_ALPHA_21164PC:
            return "Alpha 21164PC";

        case proto::DmiProcessor::FAMILY_ALPHA_21164A:
            return "Alpha 21164a";

        case proto::DmiProcessor::FAMILY_ALPHA_21264:
            return "Alpha 21264";

        case proto::DmiProcessor::FAMILY_ALPHA_21364:
            return "Alpha 21364";

        case proto::DmiProcessor::FAMILY_AMD_TURION_2_ULTRA_DUAL_CORE_MOBILE_M_FAMILY:
            return "AMD Turion II Ultra Dual-Core Mobile M";

        case proto::DmiProcessor::FAMILY_AMD_TURION_2_DUAL_CORE_MOBILE_M_FAMILY:
            return "AMD Turion II Dual-Core Mobile M";

        case proto::DmiProcessor::FAMILY_AMD_ATHLON_2_DUAL_CORE_M_FAMILY:
            return "AMD Athlon II Dual-Core M";

        case proto::DmiProcessor::FAMILY_AMD_OPTERON_6100_SERIES_PROCESSOR:
            return "AMD Opteron 6100";

        case proto::DmiProcessor::FAMILY_AMD_OPTERON_4100_SERIES_PROCESSOR:
            return "AMD Opteron 4100";

        case proto::DmiProcessor::FAMILY_AMD_OPTERON_6200_SERIES_PROCESSOR:
            return "AMD Opteron 6200";

        case proto::DmiProcessor::FAMILY_AMD_OPTERON_4200_SERIES_PROCESSOR:
            return "AMD Opteron 4200";

        case proto::DmiProcessor::FAMILY_AMD_FX_SERIES_PROCESSOR:
            return "AMD FX Series";

        case proto::DmiProcessor::FAMILY_MIPS_FAMILY:
            return "MIPS";

        case proto::DmiProcessor::FAMILY_MIPS_R4000:
            return "MIPS R4000";

        case proto::DmiProcessor::FAMILY_MIPS_R4200:
            return "MIPS R4200";

        case proto::DmiProcessor::FAMILY_MIPS_R4400:
            return "MIPS R4400";

        case proto::DmiProcessor::FAMILY_MIPS_R4600:
            return "MIPS R4600";

        case proto::DmiProcessor::FAMILY_MIPS_R10000:
            return "MIPS R10000";

        case proto::DmiProcessor::FAMILY_AMD_C_SERIES_PROCESSOR:
            return "AMD C-Series";

        case proto::DmiProcessor::FAMILY_AMD_E_SERIES_PROCESSOR:
            return "AMD E-Series";

        case proto::DmiProcessor::FAMILY_AMD_A_SERIES_PROCESSOR:
            return "AMD A-Series";

        case proto::DmiProcessor::FAMILY_AMD_G_SERIES_PROCESSOR:
            return "AMD G-Series";

        case proto::DmiProcessor::FAMILY_AMD_Z_SERIES_PROCESSOR:
            return "AMD Z-Series";

        case proto::DmiProcessor::FAMILY_AMD_R_SERIES_PROCESSOR:
            return "AMD R-Series";

        case proto::DmiProcessor::FAMILY_AMD_OPTERON_4300_SERIES_PROCESSOR:
            return "AMD Opteron 4300";

        case proto::DmiProcessor::FAMILY_AMD_OPTERON_6300_SERIES_PROCESSOR:
            return "AMD Opteron 6300";

        case proto::DmiProcessor::FAMILY_AMD_OPTERON_3300_SERIES_PROCESSOR:
            return "AMD Opteron 3300";

        case proto::DmiProcessor::FAMILY_AMD_FIREPRO_SERIES_PROCESSOR:
            return "AMD FirePro";

        case proto::DmiProcessor::FAMILY_SPARC_FAMILY:
            return "SPARC";

        case proto::DmiProcessor::FAMILY_SUPER_SPARC:
            return "SuperSPARC";

        case proto::DmiProcessor::FAMILY_MICRO_SPARC_2:
            return "microSPARC II";

        case proto::DmiProcessor::FAMILY_MICRO_SPARC_2EP:
            return "microSPARC IIep";

        case proto::DmiProcessor::FAMILY_ULTRA_SPARC:
            return "UltraSPARC";

        case proto::DmiProcessor::FAMILY_ULTRA_SPARC_2:
            return "UltraSPARC II";

        case proto::DmiProcessor::FAMILY_ULTRA_SPARC_2I:
            return "UltraSPARC IIi";

        case proto::DmiProcessor::FAMILY_ULTRA_SPARC_3:
            return "UltraSPARC III";

        case proto::DmiProcessor::FAMILY_ULTRA_SPARC_3I:
            return "UltraSPARC IIIi";

        case proto::DmiProcessor::FAMILY_68040_FAMILY:
            return "68040";

        case proto::DmiProcessor::FAMILY_68XXX:
            return "68xxx";

        case proto::DmiProcessor::FAMILY_68000:
            return "68000";

        case proto::DmiProcessor::FAMILY_68010:
            return "68010";

        case proto::DmiProcessor::FAMILY_68020:
            return "68020";

        case proto::DmiProcessor::FAMILY_68030:
            return "68030";

        case proto::DmiProcessor::FAMILY_AMD_ATHLON_X4_QUAD_CORE_PROCESSOR_FAMILY:
            return "AMD Athlon X4 Quad-Core";

        case proto::DmiProcessor::FAMILY_AMD_OPTERON_X1000_SERIES_PROCESSOR:
            return "AMD Opteron X1000 Series Processor";

        case proto::DmiProcessor::FAMILY_AMD_OPTERON_X2000_SERIES_APU:
            return "AMD Opteron X2000 Series APU";

        case proto::DmiProcessor::FAMILY_AMD_OPTERON_A_SERIES_PROCESSOR:
            return "AMD Opteron A-Series Processor";

        case proto::DmiProcessor::FAMILY_AMD_OPTERON_X3000_SERIES_APU:
            return "AMD Opteron X3000 Series APU";

        case proto::DmiProcessor::FAMILY_AMD_ZEN_PROCESSOR_FAMILY:
            return "AMD Zen";

        case proto::DmiProcessor::FAMILY_HOBBIT_FAMILY:
            return "Hobbit";

        case proto::DmiProcessor::FAMILY_CRUSOE_TM5000_FAMILY:
            return "Crusoe TM5000";

        case proto::DmiProcessor::FAMILY_CRUSOE_TM3000_FAMILY:
            return "Crusoe TM3000";

        case proto::DmiProcessor::FAMILY_EFFICEON_TM8000_FAMILY:
            return "Efficeon TM8000";

        case proto::DmiProcessor::FAMILY_WEITEK:
            return "Weitek";

        case proto::DmiProcessor::FAMILY_INTEL_ITANIUM_PROCESSOR:
            return "Intel Itanium";

        case proto::DmiProcessor::FAMILY_AMD_ATHLON_64_PROCESSOR_FAMILY:
            return "AMD Athlon 64";

        case proto::DmiProcessor::FAMILY_AMD_OPTERON_PROCESSOR_FAMILY:
            return "AMD Opteron";

        case proto::DmiProcessor::FAMILY_AMD_SEMPRON_PROCESSOR_FAMILY:
            return "AMD Sempron";

        case proto::DmiProcessor::FAMILY_AMD_TURION_64_MOBILE_TECHNOLOGY:
            return "AMD Turion 64 Mobile Technology";

        case proto::DmiProcessor::FAMILY_AMD_OPTERON_DUAL_CORE_PROCESSOR_FAMILY:
            return "AMD Opteron Dual-Core";

        case proto::DmiProcessor::FAMILY_AMD_ATHLON_64_X2_DUAL_CORE_PROCESSOR_FAMILY:
            return "AMD Athlon 64 X2 Dual-Core";

        case proto::DmiProcessor::FAMILY_AMD_TURION_64_X2_MOBILE_TECHNOLOGY:
            return "AMD Turion 64 X2 Mobile Technology";

        case proto::DmiProcessor::FAMILY_AMD_OPTERON_QUAD_CORE_PROCESSOR_FAMILY:
            return "AMD Opteron Quad-Core";

        case proto::DmiProcessor::FAMILY_AMD_OPTERON_THIRD_GEN_PROCESSOR_FAMILY:
            return "AMD Opteron Third-Generation";

        case proto::DmiProcessor::FAMILY_AMD_PHENOM_FX_QUAD_CORE_PROCESSOR_FAMILY:
            return "AMD Phenom FX Quad-Core";

        case proto::DmiProcessor::FAMILY_AMD_PHENOM_X4_QUAD_CORE_PROCESSOR_FAMILY:
            return "AMD Phenom X4 Quad-Core";

        case proto::DmiProcessor::FAMILY_AMD_PHENOM_X2_DUAL_CORE_PROCESSOR_FAMILY:
            return "AMD Phenom X2 Dual-Core";

        case proto::DmiProcessor::FAMILY_AMD_ATHLON_X2_DUAL_CORE_PROCESSOR_FAMILY:
            return "AMD Athlon X2 Dual-Core";

        case proto::DmiProcessor::FAMILY_PA_RISC_FAMILY:
            return "PA-RISC";

        case proto::DmiProcessor::FAMILY_PA_RISC_8500:
            return "PA-RISC 8500";

        case proto::DmiProcessor::FAMILY_PA_RISC_8000:
            return "PA-RISC 8000";

        case proto::DmiProcessor::FAMILY_PA_RISC_7300LC:
            return "PA-RISC 7300LC";

        case proto::DmiProcessor::FAMILY_PA_RISC_7200:
            return "PA-RISC 7200";

        case proto::DmiProcessor::FAMILY_PA_RISC_7100LC:
            return "PA-RISC 7100LC";

        case proto::DmiProcessor::FAMILY_PA_RISC_7100:
            return "PA-RISC 7100";

        case proto::DmiProcessor::FAMILY_V30_FAMILY:
            return "V30";

        case proto::DmiProcessor::FAMILY_INTEL_XEON_QUAD_CORE_3200_PROCESSOR_SERIES:
            return "Intel Xeon Quad-Core 3200";

        case proto::DmiProcessor::FAMILY_INTEL_XEON_DUAL_CORE_3000_PROCESSOR_SERIES:
            return "Intel Xeon Dual-Core 3000";

        case proto::DmiProcessor::FAMILY_INTEL_XEON_QUAD_CORE_5300_PROCESSOR_SERIES:
            return "Intel Xeon Quad-Core 5300";

        case proto::DmiProcessor::FAMILY_INTEL_XEON_DUAL_CORE_5100_PROCESSOR_SERIES:
            return "Intel Xeon Dual-Core 5100";

        case proto::DmiProcessor::FAMILY_INTEL_XEON_DUAL_CORE_5000_PROCESSOR_SERIES:
            return "Intel Xeon Dual-Core 5000";

        case proto::DmiProcessor::FAMILY_INTEL_XEON_DUAL_CORE_LV_PROCESSOR:
            return "Intel Xeon Dual-Core LV";

        case proto::DmiProcessor::FAMILY_INTEL_XEON_DUAL_CORE_ULV_PROCESSOR:
            return "Intel Xeon Dual-Core ULV";

        case proto::DmiProcessor::FAMILY_INTEL_XEON_DUAL_CORE_7100_PROCESSOR_SERIES:
            return "Intel Xeon Dual-Core 7100";

        case proto::DmiProcessor::FAMILY_INTEL_XEON_QUAD_CORE_5400_PROCESSOR_SERIES:
            return "Intel Xeon Quad-Core 5400";

        case proto::DmiProcessor::FAMILY_INTEL_XEON_QUAD_CORE_PROCESSOR:
            return "Intel Xeon Quad-Core";

        case proto::DmiProcessor::FAMILY_INTEL_XEON_DUAL_CORE_5200_PROCESSOR_SERIES:
            return "Intel Xeon Dual-Core 5200";

        case proto::DmiProcessor::FAMILY_INTEL_XEON_DUAL_CORE_7200_PROCESSOR_SERIES:
            return "Intel Xeon Dual-Core 7200";

        case proto::DmiProcessor::FAMILY_INTEL_XEON_QUAD_CORE_7300_PROCESSOR_SERIES:
            return "Intel Xeon Quad-Core 7300";

        case proto::DmiProcessor::FAMILY_INTEL_XEON_QUAD_CORE_7400_PROCESSOR_SERIES:
            return "Intel Xeon Quad-Core 7400";

        case proto::DmiProcessor::FAMILY_INTEL_XEON_MULTI_CORE_7400_PROCESSOR_SERIES:
            return "Intel Xeon Multi-Core 7400";

        case proto::DmiProcessor::FAMILY_INTEL_PENTIUM_3_XEON_PROCESSOR:
            return "Intel Pentium III Xeon";

        case proto::DmiProcessor::FAMILY_INTEL_PENTIUM_3_PROCESSOR_WITH_SPEED_STEP:
            return "Intel Pentium III with SpeedStep Technology";

        case proto::DmiProcessor::FAMILY_INTEL_PENTIUM_4_PROCESSOR:
            return "Intel Pentium 4";

        case proto::DmiProcessor::FAMILY_INTEL_XEON_PROCESSOR:
            return "Intel Xeon";

        case proto::DmiProcessor::FAMILY_AS400_FAMILY:
            return "AS400";

        case proto::DmiProcessor::FAMILY_INTEL_XEON_MP_PROCESSOR:
            return "Intel Xeon MP";

        case proto::DmiProcessor::FAMILY_AMD_ATHLON_XP_PROCESSOR_FAMILY:
            return "AMD Athlon XP";

        case proto::DmiProcessor::FAMILY_AMD_ATHLON_MP_PROCESSOR_FAMILY:
            return "AMD Athlon MP";

        case proto::DmiProcessor::FAMILY_INTEL_ITANIUM_2_PROCESSOR:
            return "Intel Itanium 2";

        case proto::DmiProcessor::FAMILY_INTEL_PENTIUM_M_PROCESSOR:
            return "Intel Pentium M";

        case proto::DmiProcessor::FAMILY_INTEL_CELERON_D_PROCESSOR:
            return "Intel Celeron D";

        case proto::DmiProcessor::FAMILY_INTEL_PENTIUM_D_PROCESSOR:
            return "Intel Pentium D";

        case proto::DmiProcessor::FAMILY_INTEL_PENTIUM_PROCESSOR_EXTREME_EDITION:
            return "Intel Pentium Extreme Edition";

        case proto::DmiProcessor::FAMILY_INTEL_CORE_SOLO_PROCESSOR:
            return "Intel Core Solo";

        case proto::DmiProcessor::FAMILY_INTEL_CORE_2_DUO_PROCESSOR:
            return "Intel Core 2 Duo";

        case proto::DmiProcessor::FAMILY_INTEL_CORE_2_SOLO_PROCESSOR:
            return "Intel Core 2 Solo";

        case proto::DmiProcessor::FAMILY_INTEL_CORE_2_EXTREME_PROCESSOR:
            return "Intel Core 2 Extreme";

        case proto::DmiProcessor::FAMILY_INTEL_CORE_2_QUAD_PROCESSOR:
            return "Intel Core 2 Quad";

        case proto::DmiProcessor::FAMILY_INTEL_CORE_2_EXTREME_MOBILE_PROCESSOR:
            return "Intel Core 2 Extreme Mobile";

        case proto::DmiProcessor::FAMILY_INTEL_CORE_2_DUO_MOBILE_PROCESSOR:
            return "Intel Core 2 Duo Mobile";

        case proto::DmiProcessor::FAMILY_INTEL_CORE_2_SOLO_MOBILE_PROCESSOR:
            return "Intel Core 2 Solo Mobile";

        case proto::DmiProcessor::FAMILY_INTEL_CORE_I7_PROCESSOR:
            return "Intel Core i7";

        case proto::DmiProcessor::FAMILY_INTEL_CELERON_DUAL_CORE_PROCESSOR:
            return "Intel Celeron Dual-Core";

        case proto::DmiProcessor::FAMILY_IBM390_FAMILY:
            return "IBM390";

        case proto::DmiProcessor::FAMILY_G4:
            return "G4";

        case proto::DmiProcessor::FAMILY_G5:
            return "G5";

        case proto::DmiProcessor::FAMILY_ESA_390_G6:
            return "ESA/390 G6";

        case proto::DmiProcessor::FAMILY_Z_ARCHITECTURE_BASE:
            return "z/Architecture Base";

        case proto::DmiProcessor::FAMILY_INTEL_CORE_I5_PROCESSOR:
            return "Intel Core i5";

        case proto::DmiProcessor::FAMILY_INTEL_CORE_I3_PROCESSOR:
            return "Intel Core i3";

        case proto::DmiProcessor::FAMILY_VIA_C7_M_PROCESSOR_FAMILY:
            return "VIA C7-M";

        case proto::DmiProcessor::FAMILY_VIA_C7_D_PROCESSOR_FAMILY:
            return "VIA C7-D";

        case proto::DmiProcessor::FAMILY_VIA_C7_PROCESSOR_FAMILY:
            return "VIA C7";

        case proto::DmiProcessor::FAMILY_VIA_EDEN_PROCESSOR_FAMILY:
            return "VIA Eden";

        case proto::DmiProcessor::FAMILY_INTEL_XEON_MULTI_CORE_PROCESSOR:
            return "Intel Xeon Multi-Core";

        case proto::DmiProcessor::FAMILY_INTEL_XEON_DUAL_CORE_3XXX_PROCESSOR_SERIES:
            return "Intel Xeon Dual-Core 3xxx";

        case proto::DmiProcessor::FAMILY_INTEL_XEON_QUAD_CORE_3XXX_PROCESSOR_SERIES:
            return "Intel Xeon Quad-Core 3xxx";

        case proto::DmiProcessor::FAMILY_VIA_NANO_PROCESSOR_FAMILY:
            return "VIA Nano";

        case proto::DmiProcessor::FAMILY_INTEL_XEON_DUAL_CORE_5XXX_PROCESSOR_SERIES:
            return "Intel Xeon Dual-Core 5xxx";

        case proto::DmiProcessor::FAMILY_INTEL_XEON_QUAD_CORE_5XXX_PROCESSOR_SERIES:
            return "Intel Xeon Quad-Core 5xxx";

        case proto::DmiProcessor::FAMILY_INTEL_XEON_DUAL_CORE_7XXX_PROCESSOR_SERIES:
            return "Intel Xeon Dual-Core 7xxx";

        case proto::DmiProcessor::FAMILY_INTEL_XEON_QUAD_CORE_7XXX_PROCESSOR_SERIES:
            return "Intel Xeon Quad-Core 7xxx";

        case proto::DmiProcessor::FAMILY_INTEL_XEON_MULTI_CORE_7XXX_PROCESSOR_SERIES:
            return "Intel Xeon Multi-Core 7xxx";

        case proto::DmiProcessor::FAMILY_INTEL_XEON_MULTI_CORE_3400_PROCESSOR_SERIES:
            return "Intel Xeon Multi-Core 3400";

        case proto::DmiProcessor::FAMILY_AMD_OPTERON_3000_PROCESSOR_SERIES:
            return "AMD Opteron 3000 Series";

        case proto::DmiProcessor::FAMILY_AMD_SEMPRON_II_PROCESSOR:
            return "AMD Sempron II";

        case proto::DmiProcessor::FAMILY_AMD_OPTERON_QUAD_CORE_EMBEDDED_PROCESSOR_FAMILY:
            return "AMD Opteron Quad-Core Embedded";

        case proto::DmiProcessor::FAMILY_AMD_PHENOM_TRIPLE_CORE_PROCESSOR_FAMILY:
            return "AMD Phenom Triple-Core";

        case proto::DmiProcessor::FAMILY_AMD_TURION_ULTRA_DUAL_CORE_MOBILE_PROCESSOR_FAMILY:
            return "AMD Turion Ultra Dual-Core Mobile";

        case proto::DmiProcessor::FAMILY_AMD_TURION_DUAL_CORE_MOBILE_PROCESSOR_FAMILY:
            return "AMD Turion Dual-Core Mobile";

        case proto::DmiProcessor::FAMILY_AMD_ATHLON_DUAL_CORE_PROCESSOR_FAMILY:
            return "AMD Athlon Dual-Core";

        case proto::DmiProcessor::FAMILY_AMD_SEMPRON_SI_PROCESSOR_FAMILY:
            return "AMD Sempron SI";

        case proto::DmiProcessor::FAMILY_AMD_PHENOM_2_PROCESSOR_FAMILY:
            return "AMD Phenom II";

        case proto::DmiProcessor::FAMILY_AMD_ATHLON_2_PROCESSOR_FAMILY:
            return "AMD Athlon II";

        case proto::DmiProcessor::FAMILY_AMD_OPTERON_SIX_CORE_PROCESSOR_FAMILY:
            return "AMD Opteron Six-Core";

        case proto::DmiProcessor::FAMILY_AMD_SEMPRON_M_PROCESSOR_FAMILY:
            return "AMD Sempron M";

        case proto::DmiProcessor::FAMILY_I860:
            return "i860";

        case proto::DmiProcessor::FAMILY_I960:
            return "i960";

        case proto::DmiProcessor::FAMILY_ARM_V7:
            return "ARMv7";

        case proto::DmiProcessor::FAMILY_ARM_V8:
            return "ARMv8";

        case proto::DmiProcessor::FAMILY_SH_3:
            return "SH-3";

        case proto::DmiProcessor::FAMILY_SH_4:
            return "SH-4";

        case proto::DmiProcessor::FAMILY_ARM:
            return "ARM";

        case proto::DmiProcessor::FAMILY_STRONG_ARM:
            return "StrongARM";

        case proto::DmiProcessor::FAMILY_6X86:
            return "6x86";

        case proto::DmiProcessor::FAMILY_MEDIA_GX:
            return "MediaGX";

        case proto::DmiProcessor::FAMILY_MII:
            return "MII";

        case proto::DmiProcessor::FAMILY_WIN_CHIP:
            return "WinChip";

        case proto::DmiProcessor::FAMILY_DSP:
            return "DSP";

        case proto::DmiProcessor::FAMILY_VIDEO_PROCESSOR:
            return "Video Processor";

        case proto::DmiProcessor::FAMILY_INTEL_CORE_2_FAMILY:
            return "Intel Core 2";

        case proto::DmiProcessor::FAMILY_AMD_K7_FAMILY:
            return "AMD K7";

        case proto::DmiProcessor::FAMILY_INTEL_CORE_2_OR_AMD_K7_FAMILY:
            return "Intel Core 2 or AMD K7";

        default:
            return "Unknown";
    }
}

const char* TypeToString(proto::DmiProcessor::Type value)
{
    switch (value)
    {
        case proto::DmiProcessor::TYPE_CENTRAL_PROCESSOR:
            return "Central Processor";

        case proto::DmiProcessor::TYPE_MATH_PROCESSOR:
            return "Math Processor";

        case proto::DmiProcessor::TYPE_DSP_PROCESSOR:
            return "DSP Processor";

        case proto::DmiProcessor::TYPE_VIDEO_PROCESSOR:
            return "Video Processor";

        case proto::DmiProcessor::TYPE_OTHER:
            return "Other Processor";

        default:
            return "Unknown";
    }
}

const char* StatusToString(proto::DmiProcessor::Status value)
{
    switch (value)
    {
        case proto::DmiProcessor::STATUS_ENABLED:
            return "Enabled";

        case proto::DmiProcessor::STATUS_DISABLED_BY_USER:
            return "Disabled by User";

        case proto::DmiProcessor::STATUS_DISABLED_BY_BIOS:
            return "Disabled by BIOS";

        case proto::DmiProcessor::STATUS_IDLE:
            return "Idle";

        case proto::DmiProcessor::STATUS_OTHER:
            return "Other";

        default:
            return "Unknown";
    }
}

const char* UpgradeToString(proto::DmiProcessor::Upgrade value)
{
    switch (value)
    {
        case proto::DmiProcessor::UPGRADE_OTHER:
            return "Other";

        case proto::DmiProcessor::UPGRADE_DAUGHTER_BOARD:
            return "Daughter Board";

        case proto::DmiProcessor::UPGRADE_ZIF_SOCKET:
            return "ZIF Socket";

        case proto::DmiProcessor::UPGRADE_REPLACEABLE_PIGGY_BACK:
            return "Replaceable Piggy Back";

        case proto::DmiProcessor::UPGRADE_NONE:
            return "None";

        case proto::DmiProcessor::UPGRADE_LIF_SOCKET:
            return "LIF Socket";

        case proto::DmiProcessor::UPGRADE_SLOT_1:
            return "Slot 1";

        case proto::DmiProcessor::UPGRADE_SLOT_2:
            return "Slot 2";

        case proto::DmiProcessor::UPGRADE_370_PIN_SOCKET:
            return "370-pin Socket";

        case proto::DmiProcessor::UPGRADE_SLOT_A:
            return "Slot A";

        case proto::DmiProcessor::UPGRADE_SLOT_M:
            return "Slot M";

        case proto::DmiProcessor::UPGRADE_SOCKET_423:
            return "Socket 423";

        case proto::DmiProcessor::UPGRADE_SOCKET_462:
            return "Socket A (Socket 462)";

        case proto::DmiProcessor::UPGRADE_SOCKET_478:
            return "Socket 478";

        case proto::DmiProcessor::UPGRADE_SOCKET_754:
            return "Socket 754";

        case proto::DmiProcessor::UPGRADE_SOCKET_940:
            return "Socket 940";

        case proto::DmiProcessor::UPGRADE_SOCKET_939:
            return "Socket 939";

        case proto::DmiProcessor::UPGRADE_SOCKET_MPGA604:
            return "Socket mPGA604";

        case proto::DmiProcessor::UPGRADE_SOCKET_LGA771:
            return "Socket LGA771";

        case proto::DmiProcessor::UPGRADE_SOCKET_LGA775:
            return "Socket LGA775";

        case proto::DmiProcessor::UPGRADE_SOCKET_S1:
            return "Socket S1";

        case proto::DmiProcessor::UPGRADE_SOCKET_AM2:
            return "Socket AM2";

        case proto::DmiProcessor::UPGRADE_SOCKET_F:
            return "Socket F (1207)";

        case proto::DmiProcessor::UPGRADE_SOCKET_LGA1366:
            return "Socket LGA1366";

        case proto::DmiProcessor::UPGRADE_SOCKET_G34:
            return "Socket G34";

        case proto::DmiProcessor::UPGRADE_SOCKET_AM3:
            return "Socket AM3";

        case proto::DmiProcessor::UPGRADE_SOCKET_C32:
            return "Socket C32";

        case proto::DmiProcessor::UPGRADE_SOCKET_LGA1156:
            return "Socket LGA1156";

        case proto::DmiProcessor::UPGRADE_SOCKET_LGA1567:
            return "Socket LGA1567";

        case proto::DmiProcessor::UPGRADE_SOCKET_PGA988A:
            return "Socket PGA988A";

        case proto::DmiProcessor::UPGRADE_SOCKET_BGA1288:
            return "Socket BGA1288";

        case proto::DmiProcessor::UPGRADE_SOCKET_RPGA988B:
            return "Socket rPGA988B";

        case proto::DmiProcessor::UPGRADE_SOCKET_BGA1023:
            return "Socket BGA1023";

        case proto::DmiProcessor::UPGRADE_SOCKET_BGA1224:
            return "Socket BGA1224";

        case proto::DmiProcessor::UPGRADE_SOCKET_BGA1155:
            return "Socket BGA1155";

        case proto::DmiProcessor::UPGRADE_SOCKET_LGA1356:
            return "Socket LGA1356";

        case proto::DmiProcessor::UPGRADE_SOCKET_LGA2011:
            return "Socket LGA2011";

        case proto::DmiProcessor::UPGRADE_SOCKET_FS1:
            return "Socket FS1";

        case proto::DmiProcessor::UPGRADE_SOCKET_FS2:
            return "Socket FS2";

        case proto::DmiProcessor::UPGRADE_SOCKET_FM1:
            return "Socket FM1";

        case proto::DmiProcessor::UPGRADE_SOCKET_FM2:
            return "Socket FM2";

        case proto::DmiProcessor::UPGRADE_SOCKET_LGA2011_3:
            return "Socket LGA2011-3";

        case proto::DmiProcessor::UPGRADE_SOCKET_LGA1356_3:
            return "Socket LGA1356-3";

        case proto::DmiProcessor::UPGRADE_SOCKET_LGA1150:
            return "Socket LGA1150";

        case proto::DmiProcessor::UPGRADE_SOCKET_BGA1168:
            return "Socket BGA1168";

        case proto::DmiProcessor::UPGRADE_SOCKET_BGA1234:
            return "Socket BGA1234";

        case proto::DmiProcessor::UPGRADE_SOCKET_BGA1364:
            return "Socket BGA1364";

        case proto::DmiProcessor::UPGRADE_SOCKET_AM4:
            return "Socket AM4";

        case proto::DmiProcessor::UPGRADE_SOCKET_LGA1151:
            return "Socket LGA1151";

        case proto::DmiProcessor::UPGRADE_SOCKET_BGA1356:
            return "Socket BGA1356";

        case proto::DmiProcessor::UPGRADE_SOCKET_BGA1440:
            return "Socket BGA1440";

        case proto::DmiProcessor::UPGRADE_SOCKET_BGA1515:
            return "Socket BGA1515";

        case proto::DmiProcessor::UPGRADE_SOCKET_LGA3647_1:
            return "Socket LGA3647-1";

        case proto::DmiProcessor::UPGRADE_SOCKET_SP3:
            return "Socket SP3";

        case proto::DmiProcessor::UPGRADE_SOCKET_SP3_R2:
            return "Socket SP3r2";

        default:
            return "Unknown";
    }
}

proto::DmiProcessor::Family GetFamily(
    const SMBios::Table& table, uint8_t major_version, uint8_t minor_version)
{
    uint8_t length = table.Length();
    uint16_t value = table.Byte(0x06);

    if (major_version == 2 && minor_version == 0 && value == 0x30)
    {
        std::string manufacturer = table.String(0x07);

        if (manufacturer.find("Intel") != std::string::npos)
            return proto::DmiProcessor::FAMILY_INTEL_PENTIUM_PRO_PROCESSOR;
    }

    if (value == 0xFE && length >= 0x2A)
        value = table.Word(0x28);

    if (value == 0xBE)
    {
        std::string manufacturer = table.String(0x07);

        if (manufacturer.find("Intel") != std::string::npos)
            return proto::DmiProcessor::FAMILY_INTEL_CORE_2_FAMILY;

        if (manufacturer.find("AMD") != std::string::npos)
            return proto::DmiProcessor::FAMILY_AMD_K7_FAMILY;

        return proto::DmiProcessor::FAMILY_INTEL_CORE_2_OR_AMD_K7_FAMILY;
    }

    switch (value)
    {
        case 0x01: return proto::DmiProcessor::FAMILY_OTHER;
        case 0x02: return proto::DmiProcessor::FAMILY_UNKNOWN;
        case 0x03: return proto::DmiProcessor::FAMILY_8086;
        case 0x04: return proto::DmiProcessor::FAMILY_80286;
        case 0x05: return proto::DmiProcessor::FAMILY_INTEL_386_PROCESSOR;
        case 0x06: return proto::DmiProcessor::FAMILY_INTEL_486_PROCESSOR;
        case 0x07: return proto::DmiProcessor::FAMILY_8087;
        case 0x08: return proto::DmiProcessor::FAMILY_80287;
        case 0x09: return proto::DmiProcessor::FAMILY_80387;
        case 0x0A: return proto::DmiProcessor::FAMILY_80487;
        case 0x0B: return proto::DmiProcessor::FAMILY_INTEL_PENTIUM_PROCESSOR;
        case 0x0C: return proto::DmiProcessor::FAMILY_INTEL_PENTIUM_PRO_PROCESSOR;
        case 0x0D: return proto::DmiProcessor::FAMILY_INTEL_PENTIUM_2_PROCESSOR;
        case 0x0E: return proto::DmiProcessor::FAMILY_PENTIUM_PROCESSOR_WITH_MMX;
        case 0x0F: return proto::DmiProcessor::FAMILY_INTEL_CELERON_PROCESSOR;
        case 0x10: return proto::DmiProcessor::FAMILY_INTEL_PENTIUM_2_XEON_PROCESSOR;
        case 0x11: return proto::DmiProcessor::FAMILY_INTEL_PENTIUM_3_PROCESSOR;
        case 0x12: return proto::DmiProcessor::FAMILY_M1_FAMILY;
        case 0x13: return proto::DmiProcessor::FAMILY_M2_FAMILY;
        case 0x14: return proto::DmiProcessor::FAMILY_INTEL_CELEROM_M_PROCESSOR;
        case 0x15: return proto::DmiProcessor::FAMILY_INTEL_PENTIUM_4_HT_PROCESSOR;

        case 0x18: return proto::DmiProcessor::FAMILY_AMD_DURON_PROCESSOR_FAMILY;
        case 0x19: return proto::DmiProcessor::FAMILY_AMD_K5_FAMILY;
        case 0x1A: return proto::DmiProcessor::FAMILY_AMD_K6_FAMILY;
        case 0x1B: return proto::DmiProcessor::FAMILY_AMD_K6_2;
        case 0x1C: return proto::DmiProcessor::FAMILY_AMD_K6_3;
        case 0x1D: return proto::DmiProcessor::FAMILY_AMD_ATHLON_PROCESSOR_FAMILY;
        case 0x1E: return proto::DmiProcessor::FAMILY_AMD_29000_FAMILY;
        case 0x1F: return proto::DmiProcessor::FAMILY_AMD_K6_2_PLUS;
        case 0x20: return proto::DmiProcessor::FAMILY_POWER_PC_FAMILY;
        case 0x21: return proto::DmiProcessor::FAMILY_POWER_PC_601;
        case 0x22: return proto::DmiProcessor::FAMILY_POWER_PC_603;
        case 0x23: return proto::DmiProcessor::FAMILY_POWER_PC_603_PLUS;
        case 0x24: return proto::DmiProcessor::FAMILY_POWER_PC_604;
        case 0x25: return proto::DmiProcessor::FAMILY_POWER_PC_620;
        case 0x26: return proto::DmiProcessor::FAMILY_POWER_PC_X704;
        case 0x27: return proto::DmiProcessor::FAMILY_POWER_PC_750;
        case 0x28: return proto::DmiProcessor::FAMILY_INTEL_CORE_DUO_PROCESSOR;
        case 0x29: return proto::DmiProcessor::FAMILY_INTEL_CORE_DUO_MOBILE_PROCESSOR;
        case 0x2A: return proto::DmiProcessor::FAMILY_INTEL_CORE_SOLO_MOBILE_PROCESSOR;
        case 0x2B: return proto::DmiProcessor::FAMILY_INTEL_ATOM_PROCESSOR;
        case 0x2C: return proto::DmiProcessor::FAMILY_INTEL_CORE_M_PROCESSOR;
        case 0x2D: return proto::DmiProcessor::FAMILY_INTEL_CORE_M3_PROCESSOR;
        case 0x2E: return proto::DmiProcessor::FAMILY_INTEL_CORE_M5_PROCESSOR;
        case 0x2F: return proto::DmiProcessor::FAMILY_INTEL_CORE_M7_PROCESSOR;
        case 0x30: return proto::DmiProcessor::FAMILY_ALPHA_FAMILY;
        case 0x31: return proto::DmiProcessor::FAMILY_ALPHA_21064;
        case 0x32: return proto::DmiProcessor::FAMILY_ALPHA_21066;
        case 0x33: return proto::DmiProcessor::FAMILY_ALPHA_21164;
        case 0x34: return proto::DmiProcessor::FAMILY_ALPHA_21164PC;
        case 0x35: return proto::DmiProcessor::FAMILY_ALPHA_21164A;
        case 0x36: return proto::DmiProcessor::FAMILY_ALPHA_21264;
        case 0x37: return proto::DmiProcessor::FAMILY_ALPHA_21364;
        case 0x38: return proto::DmiProcessor::FAMILY_AMD_TURION_2_ULTRA_DUAL_CORE_MOBILE_M_FAMILY;
        case 0x39: return proto::DmiProcessor::FAMILY_AMD_TURION_2_DUAL_CORE_MOBILE_M_FAMILY;
        case 0x3A: return proto::DmiProcessor::FAMILY_AMD_ATHLON_2_DUAL_CORE_M_FAMILY;
        case 0x3B: return proto::DmiProcessor::FAMILY_AMD_OPTERON_6100_SERIES_PROCESSOR;
        case 0x3C: return proto::DmiProcessor::FAMILY_AMD_OPTERON_4100_SERIES_PROCESSOR;
        case 0x3D: return proto::DmiProcessor::FAMILY_AMD_OPTERON_6200_SERIES_PROCESSOR;
        case 0x3E: return proto::DmiProcessor::FAMILY_AMD_OPTERON_4200_SERIES_PROCESSOR;
        case 0x3F: return proto::DmiProcessor::FAMILY_AMD_FX_SERIES_PROCESSOR;
        case 0x40: return proto::DmiProcessor::FAMILY_MIPS_FAMILY;
        case 0x41: return proto::DmiProcessor::FAMILY_MIPS_R4000;
        case 0x42: return proto::DmiProcessor::FAMILY_MIPS_R4200;
        case 0x43: return proto::DmiProcessor::FAMILY_MIPS_R4400;
        case 0x44: return proto::DmiProcessor::FAMILY_MIPS_R4600;
        case 0x45: return proto::DmiProcessor::FAMILY_MIPS_R10000;
        case 0x46: return proto::DmiProcessor::FAMILY_AMD_C_SERIES_PROCESSOR;
        case 0x47: return proto::DmiProcessor::FAMILY_AMD_E_SERIES_PROCESSOR;
        case 0x48: return proto::DmiProcessor::FAMILY_AMD_A_SERIES_PROCESSOR;
        case 0x49: return proto::DmiProcessor::FAMILY_AMD_G_SERIES_PROCESSOR;
        case 0x4A: return proto::DmiProcessor::FAMILY_AMD_Z_SERIES_PROCESSOR;
        case 0x4B: return proto::DmiProcessor::FAMILY_AMD_R_SERIES_PROCESSOR;
        case 0x4C: return proto::DmiProcessor::FAMILY_AMD_OPTERON_4300_SERIES_PROCESSOR;
        case 0x4D: return proto::DmiProcessor::FAMILY_AMD_OPTERON_6300_SERIES_PROCESSOR;
        case 0x4E: return proto::DmiProcessor::FAMILY_AMD_OPTERON_3300_SERIES_PROCESSOR;
        case 0x4F: return proto::DmiProcessor::FAMILY_AMD_FIREPRO_SERIES_PROCESSOR;
        case 0x50: return proto::DmiProcessor::FAMILY_SPARC_FAMILY;
        case 0x51: return proto::DmiProcessor::FAMILY_SUPER_SPARC;
        case 0x52: return proto::DmiProcessor::FAMILY_MICRO_SPARC_2;
        case 0x53: return proto::DmiProcessor::FAMILY_MICRO_SPARC_2EP;
        case 0x54: return proto::DmiProcessor::FAMILY_ULTRA_SPARC;
        case 0x55: return proto::DmiProcessor::FAMILY_ULTRA_SPARC_2;
        case 0x56: return proto::DmiProcessor::FAMILY_ULTRA_SPARC_2I;
        case 0x57: return proto::DmiProcessor::FAMILY_ULTRA_SPARC_3;
        case 0x58: return proto::DmiProcessor::FAMILY_ULTRA_SPARC_3I;

        case 0x60: return proto::DmiProcessor::FAMILY_68040_FAMILY;
        case 0x61: return proto::DmiProcessor::FAMILY_68XXX;
        case 0x62: return proto::DmiProcessor::FAMILY_68000;
        case 0x63: return proto::DmiProcessor::FAMILY_68010;
        case 0x64: return proto::DmiProcessor::FAMILY_68020;
        case 0x65: return proto::DmiProcessor::FAMILY_68030;
        case 0x66: return proto::DmiProcessor::FAMILY_AMD_ATHLON_X4_QUAD_CORE_PROCESSOR_FAMILY;
        case 0x67: return proto::DmiProcessor::FAMILY_AMD_OPTERON_X1000_SERIES_PROCESSOR;
        case 0x68: return proto::DmiProcessor::FAMILY_AMD_OPTERON_X2000_SERIES_APU;
        case 0x69: return proto::DmiProcessor::FAMILY_AMD_OPTERON_A_SERIES_PROCESSOR;
        case 0x6A: return proto::DmiProcessor::FAMILY_AMD_OPTERON_X3000_SERIES_APU;
        case 0x6B: return proto::DmiProcessor::FAMILY_AMD_ZEN_PROCESSOR_FAMILY;

        case 0x70: return proto::DmiProcessor::FAMILY_HOBBIT_FAMILY;

        case 0x78: return proto::DmiProcessor::FAMILY_CRUSOE_TM5000_FAMILY;
        case 0x79: return proto::DmiProcessor::FAMILY_CRUSOE_TM3000_FAMILY;
        case 0x7A: return proto::DmiProcessor::FAMILY_EFFICEON_TM8000_FAMILY;

        case 0x80: return proto::DmiProcessor::FAMILY_WEITEK;

        case 0x82: return proto::DmiProcessor::FAMILY_INTEL_ITANIUM_PROCESSOR;
        case 0x83: return proto::DmiProcessor::FAMILY_AMD_ATHLON_64_PROCESSOR_FAMILY;
        case 0x84: return proto::DmiProcessor::FAMILY_AMD_OPTERON_PROCESSOR_FAMILY;
        case 0x85: return proto::DmiProcessor::FAMILY_AMD_SEMPRON_PROCESSOR_FAMILY;
        case 0x86: return proto::DmiProcessor::FAMILY_AMD_TURION_64_MOBILE_TECHNOLOGY;
        case 0x87: return proto::DmiProcessor::FAMILY_AMD_OPTERON_DUAL_CORE_PROCESSOR_FAMILY;
        case 0x88: return proto::DmiProcessor::FAMILY_AMD_ATHLON_64_X2_DUAL_CORE_PROCESSOR_FAMILY;
        case 0x89: return proto::DmiProcessor::FAMILY_AMD_TURION_64_X2_MOBILE_TECHNOLOGY;
        case 0x8A: return proto::DmiProcessor::FAMILY_AMD_OPTERON_QUAD_CORE_PROCESSOR_FAMILY;
        case 0x8B: return proto::DmiProcessor::FAMILY_AMD_OPTERON_THIRD_GEN_PROCESSOR_FAMILY;
        case 0x8C: return proto::DmiProcessor::FAMILY_AMD_PHENOM_FX_QUAD_CORE_PROCESSOR_FAMILY;
        case 0x8D: return proto::DmiProcessor::FAMILY_AMD_PHENOM_X4_QUAD_CORE_PROCESSOR_FAMILY;
        case 0x8E: return proto::DmiProcessor::FAMILY_AMD_PHENOM_X2_DUAL_CORE_PROCESSOR_FAMILY;
        case 0x8F: return proto::DmiProcessor::FAMILY_AMD_ATHLON_X2_DUAL_CORE_PROCESSOR_FAMILY;
        case 0x90: return proto::DmiProcessor::FAMILY_PA_RISC_FAMILY;
        case 0x91: return proto::DmiProcessor::FAMILY_PA_RISC_8500;
        case 0x92: return proto::DmiProcessor::FAMILY_PA_RISC_8000;
        case 0x93: return proto::DmiProcessor::FAMILY_PA_RISC_7300LC;
        case 0x94: return proto::DmiProcessor::FAMILY_PA_RISC_7200;
        case 0x95: return proto::DmiProcessor::FAMILY_PA_RISC_7100LC;
        case 0x96: return proto::DmiProcessor::FAMILY_PA_RISC_7100;

        case 0xA0: return proto::DmiProcessor::FAMILY_V30_FAMILY;
        case 0xA1: return proto::DmiProcessor::FAMILY_INTEL_XEON_QUAD_CORE_3200_PROCESSOR_SERIES;
        case 0xA2: return proto::DmiProcessor::FAMILY_INTEL_XEON_DUAL_CORE_3000_PROCESSOR_SERIES;
        case 0xA3: return proto::DmiProcessor::FAMILY_INTEL_XEON_QUAD_CORE_5300_PROCESSOR_SERIES;
        case 0xA4: return proto::DmiProcessor::FAMILY_INTEL_XEON_DUAL_CORE_5100_PROCESSOR_SERIES;
        case 0xA5: return proto::DmiProcessor::FAMILY_INTEL_XEON_DUAL_CORE_5000_PROCESSOR_SERIES;
        case 0xA6: return proto::DmiProcessor::FAMILY_INTEL_XEON_DUAL_CORE_LV_PROCESSOR;
        case 0xA7: return proto::DmiProcessor::FAMILY_INTEL_XEON_DUAL_CORE_ULV_PROCESSOR;
        case 0xA8: return proto::DmiProcessor::FAMILY_INTEL_XEON_DUAL_CORE_7100_PROCESSOR_SERIES;
        case 0xA9: return proto::DmiProcessor::FAMILY_INTEL_XEON_QUAD_CORE_5400_PROCESSOR_SERIES;
        case 0xAA: return proto::DmiProcessor::FAMILY_INTEL_XEON_QUAD_CORE_PROCESSOR;
        case 0xAB: return proto::DmiProcessor::FAMILY_INTEL_XEON_DUAL_CORE_5200_PROCESSOR_SERIES;
        case 0xAC: return proto::DmiProcessor::FAMILY_INTEL_XEON_DUAL_CORE_7200_PROCESSOR_SERIES;
        case 0xAD: return proto::DmiProcessor::FAMILY_INTEL_XEON_QUAD_CORE_7300_PROCESSOR_SERIES;
        case 0xAE: return proto::DmiProcessor::FAMILY_INTEL_XEON_QUAD_CORE_7400_PROCESSOR_SERIES;
        case 0xAF: return proto::DmiProcessor::FAMILY_INTEL_XEON_MULTI_CORE_7400_PROCESSOR_SERIES;
        case 0xB0: return proto::DmiProcessor::FAMILY_INTEL_PENTIUM_3_XEON_PROCESSOR;
        case 0xB1: return proto::DmiProcessor::FAMILY_INTEL_PENTIUM_3_PROCESSOR_WITH_SPEED_STEP;
        case 0xB2: return proto::DmiProcessor::FAMILY_INTEL_PENTIUM_4_PROCESSOR;
        case 0xB3: return proto::DmiProcessor::FAMILY_INTEL_XEON_PROCESSOR;
        case 0xB4: return proto::DmiProcessor::FAMILY_AS400_FAMILY;
        case 0xB5: return proto::DmiProcessor::FAMILY_INTEL_XEON_MP_PROCESSOR;
        case 0xB6: return proto::DmiProcessor::FAMILY_AMD_ATHLON_XP_PROCESSOR_FAMILY;
        case 0xB7: return proto::DmiProcessor::FAMILY_AMD_ATHLON_MP_PROCESSOR_FAMILY;
        case 0xB8: return proto::DmiProcessor::FAMILY_INTEL_ITANIUM_2_PROCESSOR;
        case 0xB9: return proto::DmiProcessor::FAMILY_INTEL_PENTIUM_M_PROCESSOR;
        case 0xBA: return proto::DmiProcessor::FAMILY_INTEL_CELERON_D_PROCESSOR;
        case 0xBB: return proto::DmiProcessor::FAMILY_INTEL_PENTIUM_D_PROCESSOR;
        case 0xBC: return proto::DmiProcessor::FAMILY_INTEL_PENTIUM_PROCESSOR_EXTREME_EDITION;

        case 0xBF: return proto::DmiProcessor::FAMILY_INTEL_CORE_2_DUO_PROCESSOR;
        case 0xC0: return proto::DmiProcessor::FAMILY_INTEL_CORE_2_SOLO_PROCESSOR;
        case 0xC1: return proto::DmiProcessor::FAMILY_INTEL_CORE_2_EXTREME_PROCESSOR;
        case 0xC2: return proto::DmiProcessor::FAMILY_INTEL_CORE_2_QUAD_PROCESSOR;
        case 0xC3: return proto::DmiProcessor::FAMILY_INTEL_CORE_2_EXTREME_MOBILE_PROCESSOR;
        case 0xC4: return proto::DmiProcessor::FAMILY_INTEL_CORE_2_DUO_MOBILE_PROCESSOR;
        case 0xC5: return proto::DmiProcessor::FAMILY_INTEL_CORE_2_SOLO_MOBILE_PROCESSOR;
        case 0xC6: return proto::DmiProcessor::FAMILY_INTEL_CORE_I7_PROCESSOR;
        case 0xC7: return proto::DmiProcessor::FAMILY_INTEL_CELERON_DUAL_CORE_PROCESSOR;
        case 0xC8: return proto::DmiProcessor::FAMILY_IBM390_FAMILY;
        case 0xC9: return proto::DmiProcessor::FAMILY_G4;
        case 0xCA: return proto::DmiProcessor::FAMILY_G5;
        case 0xCB: return proto::DmiProcessor::FAMILY_ESA_390_G6;
        case 0xCC: return proto::DmiProcessor::FAMILY_Z_ARCHITECTURE_BASE;
        case 0xCD: return proto::DmiProcessor::FAMILY_INTEL_CORE_I5_PROCESSOR;
        case 0xCE: return proto::DmiProcessor::FAMILY_INTEL_CORE_I3_PROCESSOR;

        case 0xD2: return proto::DmiProcessor::FAMILY_VIA_C7_M_PROCESSOR_FAMILY;
        case 0xD3: return proto::DmiProcessor::FAMILY_VIA_C7_D_PROCESSOR_FAMILY;
        case 0xD4: return proto::DmiProcessor::FAMILY_VIA_C7_PROCESSOR_FAMILY;
        case 0xD5: return proto::DmiProcessor::FAMILY_VIA_EDEN_PROCESSOR_FAMILY;
        case 0xD6: return proto::DmiProcessor::FAMILY_INTEL_XEON_MULTI_CORE_PROCESSOR;
        case 0xD7: return proto::DmiProcessor::FAMILY_INTEL_XEON_DUAL_CORE_3XXX_PROCESSOR_SERIES;
        case 0xD8: return proto::DmiProcessor::FAMILY_INTEL_XEON_QUAD_CORE_3XXX_PROCESSOR_SERIES;
        case 0xD9: return proto::DmiProcessor::FAMILY_VIA_NANO_PROCESSOR_FAMILY;
        case 0xDA: return proto::DmiProcessor::FAMILY_INTEL_XEON_DUAL_CORE_5XXX_PROCESSOR_SERIES;
        case 0xDB: return proto::DmiProcessor::FAMILY_INTEL_XEON_QUAD_CORE_5XXX_PROCESSOR_SERIES;

        case 0xDD: return proto::DmiProcessor::FAMILY_INTEL_XEON_DUAL_CORE_7XXX_PROCESSOR_SERIES;
        case 0xDE: return proto::DmiProcessor::FAMILY_INTEL_XEON_QUAD_CORE_7XXX_PROCESSOR_SERIES;
        case 0xDF: return proto::DmiProcessor::FAMILY_INTEL_XEON_MULTI_CORE_7XXX_PROCESSOR_SERIES;
        case 0xE0: return proto::DmiProcessor::FAMILY_INTEL_XEON_MULTI_CORE_3400_PROCESSOR_SERIES;

        case 0xE4: return proto::DmiProcessor::FAMILY_AMD_OPTERON_3000_PROCESSOR_SERIES;
        case 0xE5: return proto::DmiProcessor::FAMILY_AMD_SEMPRON_II_PROCESSOR;
        case 0xE6: return proto::DmiProcessor::FAMILY_AMD_OPTERON_QUAD_CORE_EMBEDDED_PROCESSOR_FAMILY;
        case 0xE7: return proto::DmiProcessor::FAMILY_AMD_PHENOM_TRIPLE_CORE_PROCESSOR_FAMILY;
        case 0xE8: return proto::DmiProcessor::FAMILY_AMD_TURION_ULTRA_DUAL_CORE_MOBILE_PROCESSOR_FAMILY;
        case 0xE9: return proto::DmiProcessor::FAMILY_AMD_TURION_DUAL_CORE_MOBILE_PROCESSOR_FAMILY;
        case 0xEA: return proto::DmiProcessor::FAMILY_AMD_ATHLON_DUAL_CORE_PROCESSOR_FAMILY;
        case 0xEB: return proto::DmiProcessor::FAMILY_AMD_SEMPRON_SI_PROCESSOR_FAMILY;
        case 0xEC: return proto::DmiProcessor::FAMILY_AMD_PHENOM_2_PROCESSOR_FAMILY;
        case 0xED: return proto::DmiProcessor::FAMILY_AMD_ATHLON_2_PROCESSOR_FAMILY;
        case 0xEE: return proto::DmiProcessor::FAMILY_AMD_OPTERON_SIX_CORE_PROCESSOR_FAMILY;
        case 0xEF: return proto::DmiProcessor::FAMILY_AMD_SEMPRON_M_PROCESSOR_FAMILY;

        case 0xFA: return proto::DmiProcessor::FAMILY_I860;
        case 0xFB: return proto::DmiProcessor::FAMILY_I960;

        case 0x100: return proto::DmiProcessor::FAMILY_ARM_V7;
        case 0x101: return proto::DmiProcessor::FAMILY_ARM_V8;
        case 0x104: return proto::DmiProcessor::FAMILY_SH_3;
        case 0x105: return proto::DmiProcessor::FAMILY_SH_4;
        case 0x118: return proto::DmiProcessor::FAMILY_ARM;
        case 0x119: return proto::DmiProcessor::FAMILY_STRONG_ARM;
        case 0x12C: return proto::DmiProcessor::FAMILY_6X86;
        case 0x12D: return proto::DmiProcessor::FAMILY_MEDIA_GX;
        case 0x12E: return proto::DmiProcessor::FAMILY_MII;
        case 0x140: return proto::DmiProcessor::FAMILY_WIN_CHIP;
        case 0x15E: return proto::DmiProcessor::FAMILY_DSP;
        case 0x1F4: return proto::DmiProcessor::FAMILY_VIDEO_PROCESSOR;

        default:  return proto::DmiProcessor::FAMILY_UNKNOWN;
    }
}

proto::DmiProcessor::Type GetType(uint8_t value)
{
    switch (value)
    {
        case 0x01: return proto::DmiProcessor::TYPE_OTHER;
        case 0x02: return proto::DmiProcessor::TYPE_UNKNOWN;
        case 0x03: return proto::DmiProcessor::TYPE_CENTRAL_PROCESSOR;
        case 0x04: return proto::DmiProcessor::TYPE_MATH_PROCESSOR;
        case 0x05: return proto::DmiProcessor::TYPE_DSP_PROCESSOR;
        case 0x06: return proto::DmiProcessor::TYPE_VIDEO_PROCESSOR;
        default: return proto::DmiProcessor::TYPE_UNKNOWN;
    }
}

proto::DmiProcessor::Status GetStatus(uint8_t value)
{
    switch (BitSet<uint8_t>(value).Range(0, 2))
    {
        case 0x00: return proto::DmiProcessor::STATUS_UNKNOWN;
        case 0x01: return proto::DmiProcessor::STATUS_ENABLED;
        case 0x02: return proto::DmiProcessor::STATUS_DISABLED_BY_USER;
        case 0x03: return proto::DmiProcessor::STATUS_DISABLED_BY_BIOS;
        case 0x04: return proto::DmiProcessor::STATUS_IDLE;
        case 0x07: return proto::DmiProcessor::STATUS_OTHER;
        default: return proto::DmiProcessor::STATUS_UNKNOWN;
    }
}

proto::DmiProcessor::Upgrade GetUpgrade(uint8_t value)
{
    switch (value)
    {
        case 0x01: return proto::DmiProcessor::UPGRADE_OTHER;
        case 0x02: return proto::DmiProcessor::UPGRADE_UNKNOWN;
        case 0x03: return proto::DmiProcessor::UPGRADE_DAUGHTER_BOARD;
        case 0x04: return proto::DmiProcessor::UPGRADE_ZIF_SOCKET;
        case 0x05: return proto::DmiProcessor::UPGRADE_REPLACEABLE_PIGGY_BACK;
        case 0x06: return proto::DmiProcessor::UPGRADE_NONE;
        case 0x07: return proto::DmiProcessor::UPGRADE_LIF_SOCKET;
        case 0x08: return proto::DmiProcessor::UPGRADE_SLOT_1;
        case 0x09: return proto::DmiProcessor::UPGRADE_SLOT_2;
        case 0x0A: return proto::DmiProcessor::UPGRADE_370_PIN_SOCKET;
        case 0x0B: return proto::DmiProcessor::UPGRADE_SLOT_A;
        case 0x0C: return proto::DmiProcessor::UPGRADE_SLOT_M;
        case 0x0D: return proto::DmiProcessor::UPGRADE_SOCKET_423;
        case 0x0E: return proto::DmiProcessor::UPGRADE_SOCKET_462;
        case 0x0F: return proto::DmiProcessor::UPGRADE_SOCKET_478;
        case 0x10: return proto::DmiProcessor::UPGRADE_SOCKET_754;
        case 0x11: return proto::DmiProcessor::UPGRADE_SOCKET_940;
        case 0x12: return proto::DmiProcessor::UPGRADE_SOCKET_939;
        case 0x13: return proto::DmiProcessor::UPGRADE_SOCKET_MPGA604;
        case 0x14: return proto::DmiProcessor::UPGRADE_SOCKET_LGA771;
        case 0x15: return proto::DmiProcessor::UPGRADE_SOCKET_LGA775;
        case 0x16: return proto::DmiProcessor::UPGRADE_SOCKET_S1;
        case 0x17: return proto::DmiProcessor::UPGRADE_SOCKET_AM2;
        case 0x18: return proto::DmiProcessor::UPGRADE_SOCKET_F;
        case 0x19: return proto::DmiProcessor::UPGRADE_SOCKET_LGA1366;
        case 0x1A: return proto::DmiProcessor::UPGRADE_SOCKET_G34;
        case 0x1B: return proto::DmiProcessor::UPGRADE_SOCKET_AM3;
        case 0x1C: return proto::DmiProcessor::UPGRADE_SOCKET_C32;
        case 0x1D: return proto::DmiProcessor::UPGRADE_SOCKET_LGA1156;
        case 0x1E: return proto::DmiProcessor::UPGRADE_SOCKET_LGA1567;
        case 0x1F: return proto::DmiProcessor::UPGRADE_SOCKET_PGA988A;
        case 0x20: return proto::DmiProcessor::UPGRADE_SOCKET_BGA1288;
        case 0x21: return proto::DmiProcessor::UPGRADE_SOCKET_RPGA988B;
        case 0x22: return proto::DmiProcessor::UPGRADE_SOCKET_BGA1023;
        case 0x23: return proto::DmiProcessor::UPGRADE_SOCKET_BGA1224;
        case 0x24: return proto::DmiProcessor::UPGRADE_SOCKET_BGA1155;
        case 0x25: return proto::DmiProcessor::UPGRADE_SOCKET_LGA1356;
        case 0x26: return proto::DmiProcessor::UPGRADE_SOCKET_LGA2011;
        case 0x27: return proto::DmiProcessor::UPGRADE_SOCKET_FS1;
        case 0x28: return proto::DmiProcessor::UPGRADE_SOCKET_FS2;
        case 0x29: return proto::DmiProcessor::UPGRADE_SOCKET_FM1;
        case 0x2A: return proto::DmiProcessor::UPGRADE_SOCKET_FM2;
        case 0x2B: return proto::DmiProcessor::UPGRADE_SOCKET_LGA2011_3;
        case 0x2C: return proto::DmiProcessor::UPGRADE_SOCKET_LGA1356_3;
        case 0x2D: return proto::DmiProcessor::UPGRADE_SOCKET_LGA1150;
        case 0x2E: return proto::DmiProcessor::UPGRADE_SOCKET_BGA1168;
        case 0x2F: return proto::DmiProcessor::UPGRADE_SOCKET_BGA1234;
        case 0x30: return proto::DmiProcessor::UPGRADE_SOCKET_BGA1364;
        case 0x31: return proto::DmiProcessor::UPGRADE_SOCKET_AM4;
        case 0x32: return proto::DmiProcessor::UPGRADE_SOCKET_LGA1151;
        case 0x33: return proto::DmiProcessor::UPGRADE_SOCKET_BGA1356;
        case 0x34: return proto::DmiProcessor::UPGRADE_SOCKET_BGA1440;
        case 0x35: return proto::DmiProcessor::UPGRADE_SOCKET_BGA1515;
        case 0x36: return proto::DmiProcessor::UPGRADE_SOCKET_LGA3647_1;
        case 0x37: return proto::DmiProcessor::UPGRADE_SOCKET_SP3;
        case 0x38: return proto::DmiProcessor::UPGRADE_SOCKET_SP3_R2;
        default: return proto::DmiProcessor::UPGRADE_UNKNOWN;
    }
}

double GetVoltage(uint8_t value)
{
    if (!value)
        return 0.0;

    if (value & 0x80)
        return double(value & 0x7F) / 10.0;

    if (value & 0x01)
        return 5.0;

    if (value & 0x02)
        return 3.3;

    if (value & 0x04)
        return 2.9;

    return 0.0;
}

uint32_t GetCharacteristics(uint16_t value)
{
    BitSet<uint16_t> bitfield(value);
    uint32_t characteristics = 0;

    if (bitfield.Test(2))
        characteristics |= proto::DmiProcessor::CHARACTERISTIC_64BIT_CAPABLE;

    if (bitfield.Test(3))
        characteristics |= proto::DmiProcessor::CHARACTERISTIC_MULTI_CORE;

    if (bitfield.Test(4))
        characteristics |= proto::DmiProcessor::CHARACTERISTIC_HARDWARE_THREAD;

    if (bitfield.Test(5))
        characteristics |= proto::DmiProcessor::CHARACTERISTIC_EXECUTE_PROTECTION;

    if (bitfield.Test(6))
        characteristics |= proto::DmiProcessor::CHARACTERISTIC_ENHANCED_VIRTUALIZATION;

    if (bitfield.Test(7))
        characteristics |= proto::DmiProcessor::CHARACTERISTIC_POWER_CONTROL;

    return characteristics;
}

} // namespace

CategoryDmiProcessor::CategoryDmiProcessor()
    : CategoryInfo(Type::INFO_PARAM_VALUE)
{
    // Nothing
}

const char* CategoryDmiProcessor::Name() const
{
    return "Processors";
}

Category::IconId CategoryDmiProcessor::Icon() const
{
    return IDI_PROCESSOR;
}

const char* CategoryDmiProcessor::Guid() const
{
    return "{84D8B0C3-37A4-4825-A523-40B62E0CADC3}";
}

void CategoryDmiProcessor::Parse(Table& table, const std::string& data)
{
    proto::DmiProcessor message;

    if (!message.ParseFromString(data))
        return;

    table.AddColumns(ColumnList::Create()
                     .AddColumn("Parameter", 250)
                     .AddColumn("Value", 250));

    for (int index = 0; index < message.item_size(); ++index)
    {
        const proto::DmiProcessor::Item& item = message.item(index);

        Group group = table.AddGroup(StringPrintf("Processor #%d", index + 1));

        if (!item.manufacturer().empty())
            group.AddParam("Manufacturer", Value::String(item.manufacturer()));

        if (!item.version().empty())
            group.AddParam("Version", Value::String(item.version()));

        group.AddParam("Family", Value::String(FamilyToString(item.family())));
        group.AddParam("Type", Value::String(TypeToString(item.type())));
        group.AddParam("Status", Value::String(StatusToString(item.status())));

        if (!item.socket().empty())
            group.AddParam("Socket", Value::String(item.socket()));

        group.AddParam("Upgrade", Value::String(UpgradeToString(item.upgrade())));

        if (item.external_clock() != 0)
            group.AddParam("External Clock", Value::Number(item.external_clock(), "MHz"));

        if (item.current_speed() != 0)
            group.AddParam("Current Speed", Value::Number(item.current_speed(), "MHz"));

        if (item.maximum_speed() != 0)
            group.AddParam("Maximum Speed", Value::Number(item.maximum_speed(), "MHz"));

        if (item.voltage() != 0.0)
            group.AddParam("Voltage", Value::Number(item.voltage(), "V"));

        if (!item.serial_number().empty())
            group.AddParam("Serial Number", Value::String(item.serial_number()));

        if (!item.asset_tag().empty())
            group.AddParam("Asset Tag", Value::String(item.asset_tag()));

        if (!item.part_number().empty())
            group.AddParam("Part Number", Value::String(item.part_number()));

        if (item.core_count() != 0)
            group.AddParam("Core Count", Value::Number(item.core_count()));

        if (item.core_enabled() != 0)
            group.AddParam("Core Enabled", Value::Number(item.core_enabled()));

        if (item.thread_count() != 0)
            group.AddParam("Thread Count", Value::Number(item.thread_count()));

        if (item.characteristics())
        {
            Group features_group = group.AddGroup("Features");

            auto add_feature = [&](const char* name, uint32_t flag)
            {
                features_group.AddParam(name, Value::Bool(item.characteristics() & flag));
            };

            add_feature("64-bit Capable", proto::DmiProcessor::CHARACTERISTIC_64BIT_CAPABLE);
            add_feature("Multi-Core", proto::DmiProcessor::CHARACTERISTIC_MULTI_CORE);
            add_feature("Hardware Thread", proto::DmiProcessor::CHARACTERISTIC_HARDWARE_THREAD);
            add_feature("Execute Protection",proto::DmiProcessor::CHARACTERISTIC_EXECUTE_PROTECTION);
            add_feature("Enhanced Virtualization", proto::DmiProcessor::CHARACTERISTIC_ENHANCED_VIRTUALIZATION);
            add_feature("Power/Perfomance Control", proto::DmiProcessor::CHARACTERISTIC_POWER_CONTROL);
        }
    }
}

std::string CategoryDmiProcessor::Serialize()
{
    std::unique_ptr<SMBios> smbios = ReadSMBios();
    if (!smbios)
        return std::string();

    proto::DmiProcessor message;

    for (SMBios::TableEnumerator table_enumerator(*smbios, SMBios::TABLE_TYPE_PROCESSOR);
         !table_enumerator.IsAtEnd();
         table_enumerator.Advance())
    {
        SMBios::Table table = table_enumerator.GetTable();

        if (table.Length() < 0x1A)
            continue;

        proto::DmiProcessor::Item* item = message.add_item();

        item->set_manufacturer(table.String(0x07));
        item->set_version(table.String(0x10));
        item->set_family(GetFamily(table, smbios->GetMajorVersion(), smbios->GetMinorVersion()));
        item->set_type(GetType(table.Byte(0x05)));
        item->set_status(GetStatus(table.Byte(0x18)));
        item->set_socket(table.String(0x04));
        item->set_upgrade(GetUpgrade(table.Byte(0x19)));
        item->set_external_clock(table.Word(0x12));
        item->set_current_speed(table.Word(0x16));
        item->set_maximum_speed(table.Word(0x14));
        item->set_voltage(GetVoltage(table.Byte(0x11)));

        if (table.Length() >= 0x23)
        {
            item->set_serial_number(table.String(0x20));
            item->set_asset_tag(table.String(0x21));
            item->set_part_number(table.String(0x22));
        }

        if (table.Length() >= 0x28)
        {
            item->set_core_count(table.Byte(0x23));
            item->set_core_enabled(table.Byte(0x24));
            item->set_thread_count(table.Byte(0x25));
            item->set_characteristics(GetCharacteristics(table.Word(0x26)));
        }
    }

    return message.SerializeAsString();
}

} // namespace aspia
