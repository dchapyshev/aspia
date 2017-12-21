//
// PROJECT:         Aspia
// FILE:            system_info/category_smart.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/devices/physical_drive_enumerator.h"
#include "base/strings/string_printf.h"
#include "base/strings/string_util.h"
#include "base/bitset.h"
#include "system_info/category_smart.h"
#include "system_info/category_smart.pb.h"
#include "ui/resource.h"

namespace aspia {

namespace {

enum class DriveType
{
    GENERIC            = 0,
    INTEL_SSD          = 1,
    SAMSUNG_SSD        = 2,
    SAND_FORCE_SSD     = 3,
    KINGSTON_UV400_SSD = 4,
    MICRON_MU02_SSD    = 5,
    MICRON_SSD         = 6,
    PLEXTOR_SSD        = 7,
    OCZ_SSD            = 8
};

const char* GetStatusString(uint32_t attribute, uint32_t threshold, uint32_t flags)
{
    if (threshold == 0)
        return "OK. Always passed";

    if (attribute <= threshold)
    {
        if (flags & proto::SMART::Attribute::FLAG_PRE_FAILURE)
            return "WARNING. Value is pre-failure";

        return "WARNING. Value is not normal";
    }

    return "OK. Value is normal";
}

bool IsIntelSSD(const proto::SMART::Drive& drive)
{
    if (drive.attribute_size() >= 5 &&
        drive.attribute(0).id() == 0x03 &&
        drive.attribute(1).id() == 0x04 &&
        drive.attribute(2).id() == 0x05 &&
        drive.attribute(3).id() == 0x09 &&
        drive.attribute(4).id() == 0x0C)
    {
        if (drive.attribute_size() >= 8)
        {
            if (drive.attribute(5).id() == 0xC0 &&
                drive.attribute(6).id() == 0xE8 &&
                drive.attribute(7).id() == 0xE9)
            {
                return true;
            }

            if (drive.attribute(5).id() == 0xAA &&
                drive.attribute(6).id() == 0xAB &&
                drive.attribute(7).id() == 0xAC)
            {
                return true;
            }
        }

        if (drive.attribute_size() >= 7 &&
            drive.attribute(5).id() == 0xC0 &&
            drive.attribute(6).id() == 0xE1)
        {
            return true;
        }
    }

    std::string model_number = ToUpperASCII(drive.model_number());

    if (model_number.find("INTEL") != std::string::npos &&
        model_number.find("SSD") != std::string::npos)
    {
        return true;
    }

    return false;
}

bool IsMicronMU02SSD(const proto::SMART::Drive& drive)
{
    if (drive.attribute_size() >= 11 &&
        drive.attribute(0).id() == 0x01 &&
        drive.attribute(1).id() == 0x05 &&
        drive.attribute(2).id() == 0x09 &&
        drive.attribute(3).id() == 0x0C &&
        drive.attribute(4).id() == 0xA0 &&
        drive.attribute(5).id() == 0xA1 &&
        drive.attribute(6).id() == 0xA3 &&
        drive.attribute(7).id() == 0xA4 &&
        drive.attribute(8).id() == 0xA5 &&
        drive.attribute(9).id() == 0xA6 &&
        drive.attribute(10).id() == 0xA7)
    {
        return true;
    }

    if (drive.attribute_size() >= 11 &&
        drive.attribute(0).id() == 0x01 &&
        drive.attribute(1).id() == 0x05 &&
        drive.attribute(2).id() == 0x09 &&
        drive.attribute(3).id() == 0x0C &&
        drive.attribute(4).id() == 0xA0 &&
        drive.attribute(5).id() == 0xA1 &&
        drive.attribute(6).id() == 0xA3 &&
        drive.attribute(7).id() == 0x94 &&
        drive.attribute(8).id() == 0x95 &&
        drive.attribute(9).id() == 0x96 &&
        drive.attribute(10).id() == 0x97)
    {
        return true;
    }

    return false;
}

bool IsMicronSSD(const proto::SMART::Drive& drive)
{
    if (drive.attribute_size() >= 11 &&
        drive.attribute(0).id() == 0x01 &&
        drive.attribute(1).id() == 0x05 &&
        drive.attribute(2).id() == 0x09 &&
        drive.attribute(3).id() == 0x0C &&
        drive.attribute(4).id() == 0xAA &&
        drive.attribute(5).id() == 0xAB &&
        drive.attribute(6).id() == 0xAC &&
        drive.attribute(7).id() == 0xAD &&
        drive.attribute(8).id() == 0xAE &&
        drive.attribute(9).id() == 0xB5 &&
        drive.attribute(10).id() == 0xB7)
    {
        return true;
    }

    std::string model_number = ToUpperASCII(drive.model_number());

    if (model_number.find("P600") == 0 || model_number.find("C600") == 0 ||
        model_number.find("M6-") == 0  || model_number.find("M600") == 0 ||
        model_number.find("P500") == 0 || model_number.find("C500") == 0 ||
        model_number.find("M5-") == 0  || model_number.find("M500") == 0 ||
        model_number.find("P400") == 0 || model_number.find("C400") == 0 ||
        model_number.find("M4-") == 0  || model_number.find("M400") == 0 ||
        model_number.find("M3-") == 0  || model_number.find("M300") == 0 ||
        model_number.find("CRUCIAL") == 0 || model_number.find("MICRON") == 0)
    {
        return true;
    }

    return false;
}

bool IsSamsungSSD(const proto::SMART::Drive& drive)
{
    if (drive.attribute_size() >= 10 &&
        drive.attribute(0).id() == 0x05 &&
        drive.attribute(1).id() == 0x09 &&
        drive.attribute(2).id() == 0x0C &&
        drive.attribute(3).id() == 0xAA &&
        drive.attribute(4).id() == 0xAB &&
        drive.attribute(5).id() == 0xAC &&
        drive.attribute(6).id() == 0xAD &&
        drive.attribute(7).id() == 0xAE &&
        drive.attribute(8).id() == 0xB2 &&
        drive.attribute(9).id() == 0xB4)
    {
        return true;
    }

    if (drive.attribute_size() >= 5 &&
        drive.attribute(0).id() == 0x09 &&
        drive.attribute(1).id() == 0x0C &&
        drive.attribute(2).id() == 0xB2 &&
        drive.attribute(3).id() == 0xB3 &&
        drive.attribute(4).id() == 0xB4)
    {
        return true;
    }

    if (drive.attribute_size() >= 7 &&
        drive.attribute(0).id() == 0x09 &&
        drive.attribute(1).id() == 0x0C &&
        drive.attribute(2).id() == 0xB1 &&
        drive.attribute(3).id() == 0xB2 &&
        drive.attribute(4).id() == 0xB3 &&
        drive.attribute(5).id() == 0xB4 &&
        drive.attribute(6).id() == 0xB7)
    {
        return true;
    }

    if (drive.attribute_size() >= 8 &&
        drive.attribute(0).id() == 0x09 &&
        drive.attribute(1).id() == 0x0C &&
        drive.attribute(2).id() == 0xAF &&
        drive.attribute(3).id() == 0xB0 &&
        drive.attribute(4).id() == 0xB1 &&
        drive.attribute(5).id() == 0xB2 &&
        drive.attribute(6).id() == 0xB3 &&
        drive.attribute(7).id() == 0xB4)
    {
        return true;
    }

    if (drive.attribute_size() >= 7 &&
        drive.attribute(0).id() == 0x05 &&
        drive.attribute(1).id() == 0x09 &&
        drive.attribute(2).id() == 0x0C &&
        drive.attribute(3).id() == 0xB1 &&
        drive.attribute(4).id() == 0xB3 &&
        drive.attribute(5).id() == 0xB5 &&
        drive.attribute(6).id() == 0xB6)
    {
        return true;
    }

    std::string model_number = ToUpperASCII(drive.model_number());

    if (model_number.find("SAMSUNG") != std::string::npos &&
        model_number.find("SSD") != std::string::npos)
    {
        return true;
    }

    if (model_number.find("MZ-") != std::string::npos &&
        model_number.find("SSD") != std::string::npos)
    {
        return true;
    }

    return false;
}

bool IsKingstonUV400(const proto::SMART::Drive& drive)
{
    std::string model_number = ToUpperASCII(drive.model_number());

    if (model_number.find("KINGSTON") != std::string::npos &&
        model_number.find("UV400") != std::string::npos)
    {
        return true;
    }

    return false;
}

bool IsSandForceSSD(const proto::SMART::Drive& drive)
{
    if (drive.attribute_size() >= 7 &&
        drive.attribute(0).id() == 0x01 &&
        drive.attribute(1).id() == 0x05 &&
        drive.attribute(2).id() == 0x09 &&
        drive.attribute(3).id() == 0x0C &&
        drive.attribute(4).id() == 0x0D &&
        drive.attribute(5).id() == 0x64 &&
        drive.attribute(6).id() == 0xAA)
    {
        return true;
    }

    if (drive.attribute_size() >= 6 &&
        drive.attribute(0).id() == 0x01 &&
        drive.attribute(1).id() == 0x05 &&
        drive.attribute(2).id() == 0x09 &&
        drive.attribute(3).id() == 0x0C &&
        drive.attribute(4).id() == 0xAB &&
        drive.attribute(5).id() == 0xAC)
    {
        return true;
    }

    if (drive.attribute_size() >= 16 &&
        drive.attribute(0).id() == 0x01 &&
        drive.attribute(1).id() == 0x02 &&
        drive.attribute(2).id() == 0x03 &&
        drive.attribute(3).id() == 0x05 &&
        drive.attribute(4).id() == 0x07 &&
        drive.attribute(5).id() == 0x08 &&
        drive.attribute(6).id() == 0x09 &&
        drive.attribute(7).id() == 0x0A &&
        drive.attribute(8).id() == 0x0C &&
        drive.attribute(9).id() == 0xA7 &&
        drive.attribute(10).id() == 0xA8 &&
        drive.attribute(11).id() == 0xA9 &&
        drive.attribute(12).id() == 0xAA &&
        drive.attribute(13).id() == 0xAD &&
        drive.attribute(14).id() == 0xAF &&
        drive.attribute(15).id() == 0xB1)
    {
        return true;
    }

    std::string model_number = ToUpperASCII(drive.model_number());

    if (model_number.find("SANDFORCE") != std::string::npos)
        return true;

    return false;
}

bool IsPlextorSSD(const proto::SMART::Drive& drive)
{
    if (drive.attribute_size() >= 8 &&
        drive.attribute(0).id() == 0x01 &&
        drive.attribute(1).id() == 0x05 &&
        drive.attribute(2).id() == 0x09 &&
        drive.attribute(3).id() == 0x0C &&
        drive.attribute(4).id() == 0xB1 &&
        drive.attribute(5).id() == 0xB2 &&
        drive.attribute(6).id() == 0xB5 &&
        drive.attribute(7).id() == 0xB6)
    {
        return true;
    }

    std::string model_number = ToUpperASCII(drive.model_number());

    if (model_number.find("PLEXTOR") != std::string::npos ||
        model_number.find("CSSD-S6T128NM3PQ") != std::string::npos ||
        model_number.find("CSSD-S6T256NM3PQ") != std::string::npos)
    {
        return true;
    }

    return false;
}

bool IsOczSSD(const proto::SMART::Drive& drive)
{
    std::string model_number = ToUpperASCII(drive.model_number());

    if (model_number.find("OCZ-TRION") != std::string::npos)
        return true;

    if (drive.attribute_size() >= 8 &&
        drive.attribute(0).id() == 0x01 &&
        drive.attribute(1).id() == 0x03 &&
        drive.attribute(2).id() == 0x04 &&
        drive.attribute(3).id() == 0x05 &&
        drive.attribute(4).id() == 0x09 &&
        drive.attribute(5).id() == 0x0C &&
        drive.attribute(6).id() == 0xE8 &&
        drive.attribute(7).id() == 0xE9 &&
        model_number.find("OCZ") != std::string::npos)
    {
        return true;
    }

    return false;
}

DriveType GetDriveType(const proto::SMART::Drive& drive)
{
    if (IsKingstonUV400(drive))
        return DriveType::KINGSTON_UV400_SSD;
    else if (IsIntelSSD(drive))
        return DriveType::INTEL_SSD;
    else if (IsSamsungSSD(drive))
        return DriveType::SAMSUNG_SSD;
    else if (IsMicronMU02SSD(drive))
        return DriveType::MICRON_MU02_SSD;
    else if (IsMicronSSD(drive))
        return DriveType::MICRON_SSD;
    else if (IsSandForceSSD(drive))
        return DriveType::SAND_FORCE_SSD;
    else if (IsOczSSD(drive))
        return DriveType::OCZ_SSD;
    else if (IsPlextorSSD(drive))
        return DriveType::PLEXTOR_SSD;
    else
        return DriveType::GENERIC;
}

const char* GenericAttributeToString(uint32_t value)
{
    switch (value)
    {
        case 0x01: return "Raw Read Error Rate";
        case 0x02: return "Throughput Performance";
        case 0x03: return "Spin Up Time";
        case 0x04: return "Start/Stop Count";
        case 0x05: return "Reallocated Sectors Count";
        case 0x06: return "Read Channel Margin";
        case 0x07: return "Seek Error Rate";
        case 0x08: return "Seek Time Performance";
        case 0x09: return "Power-On Hours";
        case 0x0A: return "Spin Retry Count";
        case 0x0B: return "Recalibration Retries";
        case 0x0C: return "Device Power Cycle Count";
        case 0x0D: return "Soft Read Error Rate";
        case 0xB7: return "Sata Downshift Error Count";
        case 0xB8: return "End To End Error";
        case 0xB9: return "Head Stability";
        case 0xBA: return "Induced Op Vibration Detection";
        case 0xBB: return "Reported Uncorrectable Errors";
        case 0xBC: return "Command Timeout";
        case 0xBD: return "High Fly Writes";
        case 0xBE: return "Temperature Difference From 100";
        case 0xBF: return "GSense Error Rate";
        case 0xC0: return "Emergency Retract Cycle Count";
        case 0xC1: return "Load/Unload Cycle Count";
        case 0xC2: return "Temperature";
        case 0xC4: return "Reallocation Event Count";
        case 0xC5: return "Current Pending Sector Count";
        case 0xC6: return "Uncorrectable Sector Count";
        case 0xC7: return "UltraDMA CRC Error Count";
        case 0xC8: return "Write Error Rate";
        case 0xC9: return "Soft read error rate";
        case 0xCA: return "Data Address Mark errors";
        case 0xCB: return "Run out cancel";
        case 0xCC: return "Soft ECC correction";
        case 0xCD: return "Thermal asperity rate (TAR)";
        case 0xCE: return "Flying height";
        case 0xCF: return "Spin high current";
        case 0xD0: return "Spin buzz";
        case 0xD1: return "Offline seek performance";
        case 0xD3: return "Vibration During Write";
        case 0xD4: return "Shock During Write";
        case 0xDC: return "Disk Shift";
        case 0xDD: return "G-Sense Error Rate";
        case 0xDE: return "Loaded Hours";
        case 0xDF: return "Load/Unload Retry Count";
        case 0xE0: return "Load Friction";
        case 0xE1: return "Load Unload Cycle Count";
        case 0xE2: return "Load-in Time";
        case 0xE3: return "Torque Amplification Count";
        case 0xE4: return "Power-Off Retract Count";
        case 0xE6: return "GMR Head Amplitude";
        case 0xE7: return "Temperature";
        case 0xE8: return "Endurance Remaining";
        case 0xE9: return "Power On Hours";
        case 0xF0: return "Head flying hours";
        case 0xF1: return "Total Lbas Written";
        case 0xF2: return "Total Lbas Read";
        case 0xFA: return "Read error retry rate";
        case 0xFE: return "Free Fall Event Count";
        default:   return "Unknown Attribute";
    }
}

const char* IntelAttributeToString(uint32_t value)
{
    switch (value)
    {
        case 0x01: return "Read Error Rate";
        case 0x03: return "Spin UpTime";
        case 0x04: return "Start/Stop Count";
        case 0x05: return "Reallocated Sectors Count";
        case 0x09: return "Power-On Hours Count";
        case 0x0C: return "Power Cycle Count";
        case 0xAA: return "Available Reserved Space";
        case 0xAB: return "Program Fail Count";
        case 0xAC: return "Erase Fail Count";
        case 0xAE: return "Unexpected Power Loss";
        case 0xB7: return "SATA Downshift Count";
        case 0xB8: return "End-to-End Error Detection Count";
        case 0xBB: return "Uncorrectable Error Count";
        case 0xBE: return "Airflow Temperature";
        case 0xC0: return "Unsafe Shutdown Count";
        case 0xC7: return "CRC Error Count";
        case 0xE1: return "Host Writes";
        case 0xE2: return "Timed Workload Media Wear";
        case 0xE3: return "Timed Workload Host Read/Write Ratio";
        case 0xE4: return "Timed Workload Timer";
        case 0xE8: return "Available Reserved Space";
        case 0xE9: return "Media Wearout Indicator";
        case 0xF1: return "Total LBAs Written";
        case 0xF2: return "Total LBAs Read";
        case 0xF9: return "Total NAND Writes";
        default:   return "Unknown Attribute";
    }
}

const char* MicronMU02AttributeToString(uint32_t value)
{
    switch (value)
    {
        case 0x01: return "Raw Read Error Rate";
        case 0x05: return "Reallocated Sectors Count";
        case 0x09: return "Power-On Hours Count";
        case 0x0C: return "Power Cycle Count";
        case 0x0D: return "Soft Error Rate";
        case 0x0E: return "Device Capacity (NAND)";
        case 0x0F: return "User Capacity";
        case 0x10: return "Spare Blocks Available";
        case 0x11: return "Remaining Spare Blocks";
        case 0x64: return "Total Erase Count";
        case 0x94: return "Total SLC Erase Count";
        case 0x95: return "Maximum SLC Erase Count";
        case 0x96: return "Minimum SLC Erase Count";
        case 0x97: return "Average SLC Erase Count";
        case 0xA0: return "Uncorrectable Sector Count";
        case 0xA1: return "Valid Spare Blocks";
        case 0xA3: return "Initial Invalid Blocks";
        case 0xA4: return "Total Erase Count";
        case 0xA5: return "Maximum Erase Count";
        case 0xA6: return "Minimum Erase Count";
        case 0xA7: return "Average Block Erase Count";
        case 0xA8: return "Max Erase Count of Spec";
        case 0xA9: return "Percentage Lifetime Remaining";
        case 0xAA: return "Reserved Block Count";
        case 0xAB: return "Program Fail Count";
        case 0xAC: return "Erase Fail Count";
        case 0xAD: return "Average Block-Erase Count";
        case 0xAE: return "Unexpected Power Loss Count";
        case 0xAF: return "Worst Program Fail Count";
        case 0xB0: return "Worst Erase Fail Count";
        case 0xB1: return "Total Wearlevel Count";
        case 0xB2: return "Runtime Invalid Block Count";
        case 0xB4: return "Unused Reserve NAND Blocks";
        case 0xB5: return "Program Fail Count";
        case 0xB6: return "Erase Fail Count";
        case 0xB7: return "SATA Interface Downshift";
        case 0xB8: return "Error Correction Count";
        case 0xBB: return "Reported Uncorrectable Errors";
        case 0xBC: return "Command Timeout Count";
        case 0xBD: return "Factory Bad Block Count";
        case 0xC0: return "Power-Off Retract Count";
        case 0xC2: return "Enclosure Temperature";
        case 0xC3: return "Hardware ECC Recovered";
        case 0xC4: return "Reallocation Event Count";
        case 0xC5: return "Current Pending Sector Count";
        case 0xC6: return "Offline Uncorrectable Sector Count";
        case 0xC7: return "Ultra DMA CRC Error Rate";
        case 0xCA: return "Percent Lifetime Used";
        case 0xCE: return "Write Error Rate";
        case 0xD2: return "Successful RAIN Recovery Count";
        case 0xE8: return "Available Reserved Space";
        case 0xEA: return "Total Bytes Read";
        case 0xF1: return "Total LBA Write";
        case 0xF2: return "Total LBA Read";
        case 0xF3: return "ECC Bits Corrected";
        case 0xF4: return "ECC Cumulative Threshold Events";
        case 0xF5: return "Total Flash Write Count";
        case 0xF6: return "Total Host Sector Writes";
        case 0xF7: return "Host Program Page Count";
        case 0xF8: return "Background Program Page Count";
        default:   return "Unknown Attribute";
    }
}

const char* MicronAttributeToString(uint32_t value)
{
    switch (value)
    {
        case 0x01: return "Raw Read Error Rate";
        case 0x05: return "Reallocated NAND Blocks";
        case 0x09: return "Power On Hours";
        case 0x0C: return "Power Cycle Count";
        case 0x0D: return "Soft Error Rate";
        case 0x0E: return "Device Capacity (NAND)";
        case 0x0F: return "User Capacity";
        case 0x10: return "Spare Blocks Available";
        case 0x11: return "Remaining Spare Blocks";
        case 0x64: return "Total Erase Count";
        case 0xAA: return "Reserved Block Count";
        case 0xAB: return "Program Fail Count";
        case 0xAC: return "Erase Fail Count";
        case 0xAD: return "Average Block-Erase Count";
        case 0xAE: return "Unexpected Power Loss Count";
        case 0xB4: return "Unused Reserve NAND Blocks";
        case 0xB5: return "Unaligned Access Count";
        case 0xB7: return "SATA Interface Downshift";
        case 0xB8: return "Error Correction Count";
        case 0xBB: return "Reported Uncorrectable Errors";
        case 0xBC: return "Command Timeout Count";
        case 0xBD: return "Factory Bad Block Count";
        case 0xC2: return "Temperature";
        case 0xC3: return "Cumulative ECC Bit Correction Count";
        case 0xC4: return "Reallocation Event Count";
        case 0xC5: return "Current Pending Sector Count";
        case 0xC6: return "Smart Off-line Scan Uncorrectable Error Count";
        case 0xC7: return "Ultra DMA CRC Error Rate";
        case 0xCA: return "Percent Lifetime Used";
        case 0xCE: return "Write Error Rate";
        case 0xD2: return "Successful RAIN Recovery Count";
        case 0xEA: return "Total Bytes Read";
        case 0xF2: return "Write Protect Progress";
        case 0xF3: return "ECC Bits Corrected";
        case 0xF4: return "ECC Cumulative Threshold Events";
        case 0xF5: return "Cumulative Program NAND Pages";
        case 0xF6: return "Total Host Sector Writes";
        case 0xF7: return "Host Program Page Count";
        case 0xF8: return "Background Program Page Count";
        default:   return "Unknown Attribute";
    }
}

const char* SamsungAttributeToString(uint32_t value)
{
    switch (value)
    {
        case 0x05: return "Reallocated Sector Count";
        case 0x09: return "Power-On Hours Count";
        case 0x0C: return "Power-On Count";
        case 0xAF: return "Program Fail Count (Chip)";
        case 0xB0: return "Erase Fail Count (Chip)";
        case 0xB1: return "Wear Leveling Count";
        case 0xB2: return "Used Reserved Block Count (Chip)";
        case 0xB3: return "Used Reserved Block Count (Total)";
        case 0xB4: return "Unused Reserved Block Count (Total)";
        case 0xB5: return "Program Fail Count (Total)";
        case 0xB6: return "Erase Fail Count (Total)";
        case 0xB7: return "Runtime Bad Block (Total)";
        case 0xBB: return "Uncorrectable Error Count";
        case 0xBE: return "Airflow Temperature";
        case 0xC2: return "Temperature";
        case 0xC3: return "ECC Error Rate";
        case 0xC6: return "Off-Line Uncorrectable Error Count";
        case 0xC7: return "CRC Error Count";
        case 0xC9: return "Super cap Status";
        case 0xCA: return "SSD Mode Status";
        case 0xEB: return "POR Recovery Count";
        case 0xF1: return "Total LBAs Written";
        case 0xF2: return "Total LBAs Read";
        default:   return "Unknown Attribute";
    }
}

const char* KingstonUV400AttributeToString(uint32_t value)
{
    switch (value)
    {
        case 0x01: return "Read Error Rate";
        case 0x05: return "Reallocated Sector Count";
        case 0x09: return "Power On Hours";
        case 0x0C: return "Power Cycle Count";
        case 0x64: return "Total Erase Count";
        case 0xAA: return "Used Reserved Block Count";
        case 0xAB: return "Program Fail Count";
        case 0xAC: return "Erase Fail Count";
        case 0xAE: return "Unexpected Power Off Count";
        case 0xAF: return "Program Fail Count Worst Die";
        case 0xB0: return "Erase Fail Count Worst Die";
        case 0xB1: return "Wear Leveling Count";
        case 0xB2: return "Used Reserved Block Count worst Die";
        case 0xB4: return "Unused Reserved Block Count (SSD Total)";
        case 0xBB: return "Reported Uncorrectable Errors";
        case 0xC2: return "Temperature";
        case 0xC3: return "On-the-Fly ECC Uncorrectable Error Count";
        case 0xC4: return "Reallocation Event Count";
        case 0xC5: return "Pending Sector Count";
        case 0xC7: return "UDMA CRC Error Count";
        case 0xC9: return "Uncorrectable Read Error Rate";
        case 0xCC: return "Soft ECC Correction Rate";
        case 0xE7: return "SSD Life Left";
        case 0xEA: return "Total Programs";
        case 0xF1: return "GB Written from Interface";
        case 0xF2: return "GB Read from Interface";
        case 0xFA: return "Total Number of NAND Read Retries";
        default:   return "Unknown Attribute";
    }
}

const char* SandForceAttributeToString(uint32_t value)
{
    switch (value)
    {
        case 0x01: return "Raw Read Error Rate";
        case 0x05: return "Retired Block Count";
        case 0x09: return "Power-on Hours";
        case 0x0C: return "Power Cycle Count";
        case 0x0D: return "Soft Read Error Rate";
        case 0x64: return "Gigabytes Erased";
        case 0xAA: return "Reserve Block Count";
        case 0xAB: return "Program Fail Count";
        case 0xAC: return "Erase Fail Count";
        case 0xAE: return "Unexpected Power Loss Count";
        case 0xB1: return "Wear Range Delta";
        case 0xB5: return "Program Fail Count";
        case 0xB6: return "Erase Fail Count";
        case 0xB8: return "Reported I/O Error Detection Code Errors";
        case 0xBB: return "Reported Uncorrectable Errors";
        case 0xC2: return "Temperature";
        case 0xC3: return "On-the-Fly ECC Uncorrectable Error Count";
        case 0xC4: return "Reallocation Event Count";
        case 0xC6: return "Uncorrectable Sector Count";
        case 0xC7: return "SATA R-Errors (CRC) Error Count";
        case 0xC9: return "Uncorrectable Soft Read Error Rate";
        case 0xCC: return "Soft ECC Correction Rate";
        case 0xE6: return "Life Curve Status";
        case 0xE7: return "SSD Life Left";
        case 0xE8: return "Available Reserved Space";
        case 0xEB: return "SuperCap health";
        case 0xF1: return "Lifetime Writes from Host";
        case 0xF2: return "Lifetime Reads from Host";
        default:   return "Unknown Attribute";
    }
}

const char* PlextorAttributeToString(uint32_t value)
{
    switch (value)
    {
        case 0x01: return "Read Error Rate";
        case 0x05: return "Reallocated Sectors Count";
        case 0x09: return "Power-On Hours";
        case 0x0C: return "Power Cycle Count";
        case 0xAA: return "Grown Bad Blocks";
        case 0xAB: return "Program Fail Count (Total)";
        case 0xAC: return "Erase Fail Count (Total)";
        case 0xAD: return "Average Program/Erase Count (Total)";
        case 0xAE: return "Unexpected Power Loss Count";
        case 0xAF: return "Program Fail Count (Worst Case)";
        case 0xB0: return "Erase Fail Count (Worst Case)";
        case 0xB1: return "Wear Leveling Count";
        case 0xB2: return "Used Reserved Block Count (Worst Case)";
        case 0xB3: return "Used Reserved Block Count (Total)";
        case 0xB4: return "UnUsed Reserved Block Count (Total)";
        case 0xB5: return "Program Fail Count (Total)";
        case 0xB6: return "Erase Fail Count (Total)";
        case 0xB7: return "SATA Interface Down Shift";
        case 0xB8: return "End-to-End Data Errors Corrected";
        case 0xBB: return "Uncorrectable Error Count";
        case 0xBC: return "Command Time out";
        case 0xC0: return "Unsafe Shutdown Count";
        case 0xC2: return "Temperature";
        case 0xC3: return "ECC rate";
        case 0xC4: return "Reallocation Event Count";
        case 0xC6: return "Uncorrectable Sector Count";
        case 0xC7: return "CRC Error Count";
        case 0xE8: return "Available Reserved Space";
        case 0xE9: return "NAND GB written";
        case 0xF1: return "Total LBA written";
        case 0xF2: return "Total LBA read";
        default:   return "Unknown Attribute";
    }
}

const char* OczAttributeToString(uint32_t value)
{
    switch (value)
    {
        case 0x01: return "Raw Read Error Rate";
        case 0x03: return "Spin Up Time";
        case 0x04: return "Start Stop Count";
        case 0x05: return "Reallocated Sectors Count";
        case 0x09: return "Power-On Hours";
        case 0x0C: return "Power Cycle Count";
        case 0x64: return "Total Blocks Erased";
        case 0xA7: return "SSD Protect Mode";
        case 0xA8: return "SATA PHY Error Count";
        case 0xA9: return "Bad Block Count";
        case 0xAD: return "Erase Count";
        case 0xB8: return "Factory Bad Block Count";
        case 0xC0: return "Unexpected Power Loss Count";
        case 0xC2: return "Temperature";
        case 0xCA: return "Total Number of Corrected Bits";
        case 0xCD: return "Max Rated PE Counts";
        case 0xCE: return "Minimum Erase Counts";
        case 0xCF: return "Maximum Erase Counts";
        case 0xD3: return "SATA Uncorrectable Error Count";
        case 0xD4: return "NAND Page Reads During Retry";
        case 0xD5: return "Simple Read Retry Attempts";
        case 0xD6: return "Adaptive Read Retry Attempts";
        case 0xDD: return "Internal Data Path Uncorrectable Errors";
        case 0xDE: return "RAID Recovery Count";
        case 0xE6: return "Power Loss Protection";
        case 0xE8: return "Total Count of Write Sectors";
        case 0xE9: return "Remaining Life";
        case 0xF1: return "Total Host Writes";
        case 0xF2: return "Total Host Reads";
        case 0xFB: return "NAND Read Count";
        default:   return "Unknown Attribute";
    }
}

const char* AttributeToString(DriveType type, uint32_t value)
{
    switch (type)
    {
        case DriveType::GENERIC:
            return GenericAttributeToString(value);

        case DriveType::INTEL_SSD:
            return IntelAttributeToString(value);

        case DriveType::MICRON_MU02_SSD:
            return MicronMU02AttributeToString(value);

        case DriveType::MICRON_SSD:
            return MicronAttributeToString(value);

        case DriveType::SAMSUNG_SSD:
            return SamsungAttributeToString(value);

        case DriveType::KINGSTON_UV400_SSD:
            return KingstonUV400AttributeToString(value);

        case DriveType::SAND_FORCE_SSD:
            return SandForceAttributeToString(value);

        case DriveType::OCZ_SSD:
            return OczAttributeToString(value);

        case DriveType::PLEXTOR_SSD:
            return PlextorAttributeToString(value);

        default:
            return "Unknown Attribute";
    }
}

} // namespace

CategorySMART::CategorySMART()
    : CategoryInfo(Type::INFO_LIST)
{
    // Nothing
}

const char* CategorySMART::Name() const
{
    return "S.M.A.R.T.";
}

Category::IconId CategorySMART::Icon() const
{
    return IDI_DRIVE;
}

const char* CategorySMART::Guid() const
{
    return "{7B1F2ED7-7A2E-4F5C-A70B-A56AB5B8CE00}";
}

void CategorySMART::Parse(Table& table, const std::string& data)
{
    proto::SMART message;

    if (!message.ParseFromString(data))
        return;

    table.AddColumns(ColumnList::Create()
                     .AddColumn("Attribute", 250)
                     .AddColumn("Threshold", 70)
                     .AddColumn("Value", 70)
                     .AddColumn("Worst", 70)
                     .AddColumn("RAW", 100)
                     .AddColumn("Status", 120));

    for (int index = 0; index < message.drive_size(); ++index)
    {
        const proto::SMART::Drive& drive = message.drive(index);

        Group group = table.AddGroup(drive.model_number());
        DriveType type = GetDriveType(drive);

        for (int i = 0; i < drive.attribute_size(); ++i)
        {
            const proto::SMART::Attribute& attribute = drive.attribute(i);

            Row row = group.AddRow();

            row.AddValue(Value::String(
                StringPrintf("%02X %s", attribute.id(), AttributeToString(type, attribute.id()))));

            row.AddValue(Value::Number(attribute.threshold()));
            row.AddValue(Value::Number(attribute.value()));
            row.AddValue(Value::Number(attribute.worst_value()));
            row.AddValue(Value::String(StringPrintf("%012llX", attribute.raw())));

            row.AddValue(Value::String(GetStatusString(attribute.value(),
                                                       attribute.threshold(),
                                                       attribute.flags())));
        }
    }
}

std::string CategorySMART::Serialize()
{
    proto::SMART message;

    for (PhysicalDriveEnumerator enumerator; !enumerator.IsAtEnd(); enumerator.Advance())
    {
        SmartAttributeData attributes;
        memset(&attributes, 0, sizeof(attributes));

        SmartThresholdData thresholds;
        memset(&thresholds, 0, sizeof(thresholds));

        if (!enumerator.GetSmartData(attributes, thresholds))
            continue;

        proto::SMART::Drive* drive = nullptr;

        for (size_t i = 0; i < kMaxSmartAttributesCount; ++i)
        {
            if (attributes.attribute[i].id == 0)
                continue;

            if (!drive)
            {
                drive = message.add_drive();
                drive->set_model_number(enumerator.GetModelNumber());
            }

            proto::SMART::Attribute* attribute = drive->add_attribute();

            uint32_t flags = 0;

            if (BitSet<uint16_t>(attributes.attribute[i].status_flags).Test(0))
                flags |= proto::SMART::Attribute::FLAG_PRE_FAILURE;

            attribute->set_flags(flags);
            attribute->set_id(attributes.attribute[i].id);
            attribute->set_value(attributes.attribute[i].value);
            attribute->set_worst_value(attributes.attribute[i].worst_value);
            attribute->set_threshold(thresholds.threshold[i].warranty_threshold);

            uint64_t raw = 0ULL;
            memcpy(&raw, attributes.attribute[i].raw_value, 6);

            attribute->set_raw(raw);
        }
    }

    return message.SerializeAsString();
}

} // namespace aspia
