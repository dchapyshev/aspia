//
// PROJECT:         Aspia
// FILE:            base/devices/smbios.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/devices/smbios.h"
#include "base/strings/string_printf.h"
#include "base/strings/string_util.h"
#include "base/bitset.h"
#include "base/logging.h"

namespace aspia {

// static
std::unique_ptr<SMBios> SMBios::Create(std::unique_ptr<uint8_t[]> data, size_t data_size)
{
    if (!data || !data_size)
        return nullptr;

    Data* smbios = reinterpret_cast<Data*>(data.get());

    if (!GetTableCount(smbios->smbios_table_data, smbios->length))
    {
        LOG(WARNING) << "SMBios tables not found";
        return nullptr;
    }

    return std::unique_ptr<SMBios>(new SMBios(std::move(data), data_size));
}

SMBios::SMBios(std::unique_ptr<uint8_t[]> data, size_t data_size)
    : data_(std::move(data)),
      data_size_(data_size)
{
    DCHECK(data_ != nullptr);
    DCHECK(data_size_ != 0);
}

uint8_t SMBios::GetMajorVersion() const
{
    Data* smbios = reinterpret_cast<Data*>(data_.get());
    return smbios->smbios_major_version;
}

uint8_t SMBios::GetMinorVersion() const
{
    Data* smbios = reinterpret_cast<Data*>(data_.get());
    return smbios->smbios_minor_version;
}

// static
int SMBios::GetTableCount(const uint8_t* table_data, uint32_t length)
{
    int count = 0;

    const uint8_t* pos = table_data;
    const uint8_t* end = table_data + length;

    while (pos < end)
    {
        // Increase the position by the length of the structure.
        pos += pos[1];

        // Increses the offset to point to the next header that's after the strings at the end
        // of the structure.
        while (*reinterpret_cast<const uint16_t*>(pos) != 0 && pos < end)
            ++pos;

        // Points to the next stucture thas after two null bytes at the end of the strings.
        pos += 2;

        ++count;
    }

    return count;
}

//
// TableEnumeratorImpl
//

SMBios::TableEnumeratorImpl::TableEnumeratorImpl(const Data* data, uint8_t type)
    : data_(data)
{
    start_ = &data_->smbios_table_data[0];
    end_ = start_ + data_->length;
    current_ = start_;
    next_ = start_;

    Advance(type);
}

bool SMBios::TableEnumeratorImpl::IsAtEnd() const
{
    return current_ == nullptr;
}

void SMBios::TableEnumeratorImpl::Advance(uint8_t type)
{
    current_ = next_;
    DCHECK(current_);

    while (current_ + 4 <= end_)
    {
        const uint8_t table_type = current_[0];
        const uint8_t table_length = current_[1];

        if (table_length < 4)
        {
            // If a short entry is found(less than 4 bytes), not only it is invalid, but we
            // cannot reliably locate the next entry. Better stop at this point, and let the
            // user know his / her table is broken.
            LOG(WARNING) << "Invalid SMBIOS table length: " << table_length;
            break;
        }

        // Stop decoding at end of table marker.
        if (table_type == 127)
            break;

        // Look for the next handle.
        next_ = current_ + table_length;
        while (static_cast<uintptr_t>(next_ - start_ + 1) < data_->length &&
            (next_[0] != 0 || next_[1] != 0))
        {
            ++next_;
        }

        // Points to the next table thas after two null bytes at the end of the strings.
        next_ += 2;

        // The table of the specified type is found.
        if (table_type == type)
            return;

        current_ = next_;
    }

    current_ = nullptr;
    next_ = nullptr;
}

const SMBios::Data* SMBios::TableEnumeratorImpl::GetSMBiosData() const
{
    return data_;
}

const uint8_t* SMBios::TableEnumeratorImpl::GetTableData() const
{
    return current_;
}

//
// SMBiosTable
//

SMBios::TableReader::TableReader(const TableReader& other)
    : smbios_(other.smbios_),
      table_(other.table_)
{
    // Nothing
}

SMBios::TableReader::TableReader(const Data* smbios, const uint8_t* table)
    : smbios_(smbios),
      table_(table)
{
    DCHECK(smbios_);
    DCHECK(table_);
}

SMBios::TableReader& SMBios::TableReader::operator=(const TableReader& other)
{
    smbios_ = other.smbios_;
    table_ = other.table_;
    return *this;
}

uint8_t SMBios::TableReader::GetByte(uint8_t offset) const
{
    return table_[offset];
}

uint16_t SMBios::TableReader::GetWord(uint8_t offset) const
{
    return *reinterpret_cast<const uint16_t*>(GetPointer(offset));
}

uint32_t SMBios::TableReader::GetDword(uint8_t offset) const
{
    return *reinterpret_cast<const uint32_t*>(GetPointer(offset));
}

uint64_t SMBios::TableReader::GetQword(uint8_t offset) const
{
    return *reinterpret_cast<const uint64_t*>(GetPointer(offset));
}

std::string SMBios::TableReader::GetString(uint8_t offset) const
{
    uint8_t handle = GetByte(offset);
    if (!handle)
        return std::string();

    char* string = reinterpret_cast<char*>(
        const_cast<uint8_t*>(GetPointer(0))) + GetTableLength();

    while (handle > 1 && *string)
    {
        string += strlen(string);
        ++string;
        --handle;
    }

    if (!*string || !string[0])
        return std::string();

    std::string output;
    TrimWhitespaceASCII(string, TRIM_ALL, output);
    return output;
}

const uint8_t* SMBios::TableReader::GetPointer(uint8_t offset) const
{
    return &table_[offset];
}

uint8_t SMBios::TableReader::GetTableLength() const
{
    return GetByte(0x01);
}

//
// ProcessorTable
//

SMBios::ProcessorTable::ProcessorTable(const TableReader& reader)
    : reader_(reader)
{
    // Nothing
}

std::string SMBios::ProcessorTable::GetManufacturer() const
{
    if (reader_.GetTableLength() < 0x1A)
        return std::string();

    return reader_.GetString(0x07);
}

std::string SMBios::ProcessorTable::GetVersion() const
{
    if (reader_.GetTableLength() < 0x1A)
        return std::string();

    return reader_.GetString(0x10);
}

proto::DmiProcessors::Family SMBios::ProcessorTable::GetFamily() const
{
    if (reader_.GetTableLength() < 0x1A)
        return proto::DmiProcessors::FAMILY_UNKNOWN;

    uint8_t length = reader_.GetTableLength();
    uint16_t value = reader_.GetByte(0x06);

    if (reader_.GetMajorVersion() == 2 && reader_.GetMinorVersion() == 0 && value == 0x30)
    {
        std::string manufacturer = GetManufacturer();

        if (manufacturer.find("Intel") != std::string::npos)
            return proto::DmiProcessors::FAMILY_INTEL_PENTIUM_PRO_PROCESSOR;
    }

    if (value == 0xFE && length >= 0x2A)
        value = reader_.GetWord(0x28);

    if (value == 0xBE)
    {
        std::string manufacturer = GetManufacturer();

        if (manufacturer.find("Intel") != std::string::npos)
            return proto::DmiProcessors::FAMILY_INTEL_CORE_2_FAMILY;

        if (manufacturer.find("AMD") != std::string::npos)
            return proto::DmiProcessors::FAMILY_AMD_K7_FAMILY;

        return proto::DmiProcessors::FAMILY_INTEL_CORE_2_OR_AMD_K7_FAMILY;
    }

    switch (value)
    {
        case 0x01: return proto::DmiProcessors::FAMILY_OTHER;
        case 0x02: return proto::DmiProcessors::FAMILY_UNKNOWN;
        case 0x03: return proto::DmiProcessors::FAMILY_8086;
        case 0x04: return proto::DmiProcessors::FAMILY_80286;
        case 0x05: return proto::DmiProcessors::FAMILY_INTEL_386_PROCESSOR;
        case 0x06: return proto::DmiProcessors::FAMILY_INTEL_486_PROCESSOR;
        case 0x07: return proto::DmiProcessors::FAMILY_8087;
        case 0x08: return proto::DmiProcessors::FAMILY_80287;
        case 0x09: return proto::DmiProcessors::FAMILY_80387;
        case 0x0A: return proto::DmiProcessors::FAMILY_80487;
        case 0x0B: return proto::DmiProcessors::FAMILY_INTEL_PENTIUM_PROCESSOR;
        case 0x0C: return proto::DmiProcessors::FAMILY_INTEL_PENTIUM_PRO_PROCESSOR;
        case 0x0D: return proto::DmiProcessors::FAMILY_INTEL_PENTIUM_2_PROCESSOR;
        case 0x0E: return proto::DmiProcessors::FAMILY_PENTIUM_PROCESSOR_WITH_MMX;
        case 0x0F: return proto::DmiProcessors::FAMILY_INTEL_CELERON_PROCESSOR;
        case 0x10: return proto::DmiProcessors::FAMILY_INTEL_PENTIUM_2_XEON_PROCESSOR;
        case 0x11: return proto::DmiProcessors::FAMILY_INTEL_PENTIUM_3_PROCESSOR;
        case 0x12: return proto::DmiProcessors::FAMILY_M1_FAMILY;
        case 0x13: return proto::DmiProcessors::FAMILY_M2_FAMILY;
        case 0x14: return proto::DmiProcessors::FAMILY_INTEL_CELEROM_M_PROCESSOR;
        case 0x15: return proto::DmiProcessors::FAMILY_INTEL_PENTIUM_4_HT_PROCESSOR;

        case 0x18: return proto::DmiProcessors::FAMILY_AMD_DURON_PROCESSOR_FAMILY;
        case 0x19: return proto::DmiProcessors::FAMILY_AMD_K5_FAMILY;
        case 0x1A: return proto::DmiProcessors::FAMILY_AMD_K6_FAMILY;
        case 0x1B: return proto::DmiProcessors::FAMILY_AMD_K6_2;
        case 0x1C: return proto::DmiProcessors::FAMILY_AMD_K6_3;
        case 0x1D: return proto::DmiProcessors::FAMILY_AMD_ATHLON_PROCESSOR_FAMILY;
        case 0x1E: return proto::DmiProcessors::FAMILY_AMD_29000_FAMILY;
        case 0x1F: return proto::DmiProcessors::FAMILY_AMD_K6_2_PLUS;
        case 0x20: return proto::DmiProcessors::FAMILY_POWER_PC_FAMILY;
        case 0x21: return proto::DmiProcessors::FAMILY_POWER_PC_601;
        case 0x22: return proto::DmiProcessors::FAMILY_POWER_PC_603;
        case 0x23: return proto::DmiProcessors::FAMILY_POWER_PC_603_PLUS;
        case 0x24: return proto::DmiProcessors::FAMILY_POWER_PC_604;
        case 0x25: return proto::DmiProcessors::FAMILY_POWER_PC_620;
        case 0x26: return proto::DmiProcessors::FAMILY_POWER_PC_X704;
        case 0x27: return proto::DmiProcessors::FAMILY_POWER_PC_750;
        case 0x28: return proto::DmiProcessors::FAMILY_INTEL_CORE_DUO_PROCESSOR;
        case 0x29: return proto::DmiProcessors::FAMILY_INTEL_CORE_DUO_MOBILE_PROCESSOR;
        case 0x2A: return proto::DmiProcessors::FAMILY_INTEL_CORE_SOLO_MOBILE_PROCESSOR;
        case 0x2B: return proto::DmiProcessors::FAMILY_INTEL_ATOM_PROCESSOR;
        case 0x2C: return proto::DmiProcessors::FAMILY_INTEL_CORE_M_PROCESSOR;
        case 0x2D: return proto::DmiProcessors::FAMILY_INTEL_CORE_M3_PROCESSOR;
        case 0x2E: return proto::DmiProcessors::FAMILY_INTEL_CORE_M5_PROCESSOR;
        case 0x2F: return proto::DmiProcessors::FAMILY_INTEL_CORE_M7_PROCESSOR;
        case 0x30: return proto::DmiProcessors::FAMILY_ALPHA_FAMILY;
        case 0x31: return proto::DmiProcessors::FAMILY_ALPHA_21064;
        case 0x32: return proto::DmiProcessors::FAMILY_ALPHA_21066;
        case 0x33: return proto::DmiProcessors::FAMILY_ALPHA_21164;
        case 0x34: return proto::DmiProcessors::FAMILY_ALPHA_21164PC;
        case 0x35: return proto::DmiProcessors::FAMILY_ALPHA_21164A;
        case 0x36: return proto::DmiProcessors::FAMILY_ALPHA_21264;
        case 0x37: return proto::DmiProcessors::FAMILY_ALPHA_21364;
        case 0x38: return proto::DmiProcessors::FAMILY_AMD_TURION_2_ULTRA_DUAL_CORE_MOBILE_M_FAMILY;
        case 0x39: return proto::DmiProcessors::FAMILY_AMD_TURION_2_DUAL_CORE_MOBILE_M_FAMILY;
        case 0x3A: return proto::DmiProcessors::FAMILY_AMD_ATHLON_2_DUAL_CORE_M_FAMILY;
        case 0x3B: return proto::DmiProcessors::FAMILY_AMD_OPTERON_6100_SERIES_PROCESSOR;
        case 0x3C: return proto::DmiProcessors::FAMILY_AMD_OPTERON_4100_SERIES_PROCESSOR;
        case 0x3D: return proto::DmiProcessors::FAMILY_AMD_OPTERON_6200_SERIES_PROCESSOR;
        case 0x3E: return proto::DmiProcessors::FAMILY_AMD_OPTERON_4200_SERIES_PROCESSOR;
        case 0x3F: return proto::DmiProcessors::FAMILY_AMD_FX_SERIES_PROCESSOR;
        case 0x40: return proto::DmiProcessors::FAMILY_MIPS_FAMILY;
        case 0x41: return proto::DmiProcessors::FAMILY_MIPS_R4000;
        case 0x42: return proto::DmiProcessors::FAMILY_MIPS_R4200;
        case 0x43: return proto::DmiProcessors::FAMILY_MIPS_R4400;
        case 0x44: return proto::DmiProcessors::FAMILY_MIPS_R4600;
        case 0x45: return proto::DmiProcessors::FAMILY_MIPS_R10000;
        case 0x46: return proto::DmiProcessors::FAMILY_AMD_C_SERIES_PROCESSOR;
        case 0x47: return proto::DmiProcessors::FAMILY_AMD_E_SERIES_PROCESSOR;
        case 0x48: return proto::DmiProcessors::FAMILY_AMD_A_SERIES_PROCESSOR;
        case 0x49: return proto::DmiProcessors::FAMILY_AMD_G_SERIES_PROCESSOR;
        case 0x4A: return proto::DmiProcessors::FAMILY_AMD_Z_SERIES_PROCESSOR;
        case 0x4B: return proto::DmiProcessors::FAMILY_AMD_R_SERIES_PROCESSOR;
        case 0x4C: return proto::DmiProcessors::FAMILY_AMD_OPTERON_4300_SERIES_PROCESSOR;
        case 0x4D: return proto::DmiProcessors::FAMILY_AMD_OPTERON_6300_SERIES_PROCESSOR;
        case 0x4E: return proto::DmiProcessors::FAMILY_AMD_OPTERON_3300_SERIES_PROCESSOR;
        case 0x4F: return proto::DmiProcessors::FAMILY_AMD_FIREPRO_SERIES_PROCESSOR;
        case 0x50: return proto::DmiProcessors::FAMILY_SPARC_FAMILY;
        case 0x51: return proto::DmiProcessors::FAMILY_SUPER_SPARC;
        case 0x52: return proto::DmiProcessors::FAMILY_MICRO_SPARC_2;
        case 0x53: return proto::DmiProcessors::FAMILY_MICRO_SPARC_2EP;
        case 0x54: return proto::DmiProcessors::FAMILY_ULTRA_SPARC;
        case 0x55: return proto::DmiProcessors::FAMILY_ULTRA_SPARC_2;
        case 0x56: return proto::DmiProcessors::FAMILY_ULTRA_SPARC_2I;
        case 0x57: return proto::DmiProcessors::FAMILY_ULTRA_SPARC_3;
        case 0x58: return proto::DmiProcessors::FAMILY_ULTRA_SPARC_3I;

        case 0x60: return proto::DmiProcessors::FAMILY_68040_FAMILY;
        case 0x61: return proto::DmiProcessors::FAMILY_68XXX;
        case 0x62: return proto::DmiProcessors::FAMILY_68000;
        case 0x63: return proto::DmiProcessors::FAMILY_68010;
        case 0x64: return proto::DmiProcessors::FAMILY_68020;
        case 0x65: return proto::DmiProcessors::FAMILY_68030;
        case 0x66: return proto::DmiProcessors::FAMILY_AMD_ATHLON_X4_QUAD_CORE_PROCESSOR_FAMILY;
        case 0x67: return proto::DmiProcessors::FAMILY_AMD_OPTERON_X1000_SERIES_PROCESSOR;
        case 0x68: return proto::DmiProcessors::FAMILY_AMD_OPTERON_X2000_SERIES_APU;
        case 0x69: return proto::DmiProcessors::FAMILY_AMD_OPTERON_A_SERIES_PROCESSOR;
        case 0x6A: return proto::DmiProcessors::FAMILY_AMD_OPTERON_X3000_SERIES_APU;
        case 0x6B: return proto::DmiProcessors::FAMILY_AMD_ZEN_PROCESSOR_FAMILY;

        case 0x70: return proto::DmiProcessors::FAMILY_HOBBIT_FAMILY;

        case 0x78: return proto::DmiProcessors::FAMILY_CRUSOE_TM5000_FAMILY;
        case 0x79: return proto::DmiProcessors::FAMILY_CRUSOE_TM3000_FAMILY;
        case 0x7A: return proto::DmiProcessors::FAMILY_EFFICEON_TM8000_FAMILY;

        case 0x80: return proto::DmiProcessors::FAMILY_WEITEK;

        case 0x82: return proto::DmiProcessors::FAMILY_INTEL_ITANIUM_PROCESSOR;
        case 0x83: return proto::DmiProcessors::FAMILY_AMD_ATHLON_64_PROCESSOR_FAMILY;
        case 0x84: return proto::DmiProcessors::FAMILY_AMD_OPTERON_PROCESSOR_FAMILY;
        case 0x85: return proto::DmiProcessors::FAMILY_AMD_SEMPRON_PROCESSOR_FAMILY;
        case 0x86: return proto::DmiProcessors::FAMILY_AMD_TURION_64_MOBILE_TECHNOLOGY;
        case 0x87: return proto::DmiProcessors::FAMILY_AMD_OPTERON_DUAL_CORE_PROCESSOR_FAMILY;
        case 0x88: return proto::DmiProcessors::FAMILY_AMD_ATHLON_64_X2_DUAL_CORE_PROCESSOR_FAMILY;
        case 0x89: return proto::DmiProcessors::FAMILY_AMD_TURION_64_X2_MOBILE_TECHNOLOGY;
        case 0x8A: return proto::DmiProcessors::FAMILY_AMD_OPTERON_QUAD_CORE_PROCESSOR_FAMILY;
        case 0x8B: return proto::DmiProcessors::FAMILY_AMD_OPTERON_THIRD_GEN_PROCESSOR_FAMILY;
        case 0x8C: return proto::DmiProcessors::FAMILY_AMD_PHENOM_FX_QUAD_CORE_PROCESSOR_FAMILY;
        case 0x8D: return proto::DmiProcessors::FAMILY_AMD_PHENOM_X4_QUAD_CORE_PROCESSOR_FAMILY;
        case 0x8E: return proto::DmiProcessors::FAMILY_AMD_PHENOM_X2_DUAL_CORE_PROCESSOR_FAMILY;
        case 0x8F: return proto::DmiProcessors::FAMILY_AMD_ATHLON_X2_DUAL_CORE_PROCESSOR_FAMILY;
        case 0x90: return proto::DmiProcessors::FAMILY_PA_RISC_FAMILY;
        case 0x91: return proto::DmiProcessors::FAMILY_PA_RISC_8500;
        case 0x92: return proto::DmiProcessors::FAMILY_PA_RISC_8000;
        case 0x93: return proto::DmiProcessors::FAMILY_PA_RISC_7300LC;
        case 0x94: return proto::DmiProcessors::FAMILY_PA_RISC_7200;
        case 0x95: return proto::DmiProcessors::FAMILY_PA_RISC_7100LC;
        case 0x96: return proto::DmiProcessors::FAMILY_PA_RISC_7100;

        case 0xA0: return proto::DmiProcessors::FAMILY_V30_FAMILY;
        case 0xA1: return proto::DmiProcessors::FAMILY_INTEL_XEON_QUAD_CORE_3200_PROCESSOR_SERIES;
        case 0xA2: return proto::DmiProcessors::FAMILY_INTEL_XEON_DUAL_CORE_3000_PROCESSOR_SERIES;
        case 0xA3: return proto::DmiProcessors::FAMILY_INTEL_XEON_QUAD_CORE_5300_PROCESSOR_SERIES;
        case 0xA4: return proto::DmiProcessors::FAMILY_INTEL_XEON_DUAL_CORE_5100_PROCESSOR_SERIES;
        case 0xA5: return proto::DmiProcessors::FAMILY_INTEL_XEON_DUAL_CORE_5000_PROCESSOR_SERIES;
        case 0xA6: return proto::DmiProcessors::FAMILY_INTEL_XEON_DUAL_CORE_LV_PROCESSOR;
        case 0xA7: return proto::DmiProcessors::FAMILY_INTEL_XEON_DUAL_CORE_ULV_PROCESSOR;
        case 0xA8: return proto::DmiProcessors::FAMILY_INTEL_XEON_DUAL_CORE_7100_PROCESSOR_SERIES;
        case 0xA9: return proto::DmiProcessors::FAMILY_INTEL_XEON_QUAD_CORE_5400_PROCESSOR_SERIES;
        case 0xAA: return proto::DmiProcessors::FAMILY_INTEL_XEON_QUAD_CORE_PROCESSOR;
        case 0xAB: return proto::DmiProcessors::FAMILY_INTEL_XEON_DUAL_CORE_5200_PROCESSOR_SERIES;
        case 0xAC: return proto::DmiProcessors::FAMILY_INTEL_XEON_DUAL_CORE_7200_PROCESSOR_SERIES;
        case 0xAD: return proto::DmiProcessors::FAMILY_INTEL_XEON_QUAD_CORE_7300_PROCESSOR_SERIES;
        case 0xAE: return proto::DmiProcessors::FAMILY_INTEL_XEON_QUAD_CORE_7400_PROCESSOR_SERIES;
        case 0xAF: return proto::DmiProcessors::FAMILY_INTEL_XEON_MULTI_CORE_7400_PROCESSOR_SERIES;
        case 0xB0: return proto::DmiProcessors::FAMILY_INTEL_PENTIUM_3_XEON_PROCESSOR;
        case 0xB1: return proto::DmiProcessors::FAMILY_INTEL_PENTIUM_3_PROCESSOR_WITH_SPEED_STEP;
        case 0xB2: return proto::DmiProcessors::FAMILY_INTEL_PENTIUM_4_PROCESSOR;
        case 0xB3: return proto::DmiProcessors::FAMILY_INTEL_XEON_PROCESSOR;
        case 0xB4: return proto::DmiProcessors::FAMILY_AS400_FAMILY;
        case 0xB5: return proto::DmiProcessors::FAMILY_INTEL_XEON_MP_PROCESSOR;
        case 0xB6: return proto::DmiProcessors::FAMILY_AMD_ATHLON_XP_PROCESSOR_FAMILY;
        case 0xB7: return proto::DmiProcessors::FAMILY_AMD_ATHLON_MP_PROCESSOR_FAMILY;
        case 0xB8: return proto::DmiProcessors::FAMILY_INTEL_ITANIUM_2_PROCESSOR;
        case 0xB9: return proto::DmiProcessors::FAMILY_INTEL_PENTIUM_M_PROCESSOR;
        case 0xBA: return proto::DmiProcessors::FAMILY_INTEL_CELERON_D_PROCESSOR;
        case 0xBB: return proto::DmiProcessors::FAMILY_INTEL_PENTIUM_D_PROCESSOR;
        case 0xBC: return proto::DmiProcessors::FAMILY_INTEL_PENTIUM_PROCESSOR_EXTREME_EDITION;

        case 0xBF: return proto::DmiProcessors::FAMILY_INTEL_CORE_2_DUO_PROCESSOR;
        case 0xC0: return proto::DmiProcessors::FAMILY_INTEL_CORE_2_SOLO_PROCESSOR;
        case 0xC1: return proto::DmiProcessors::FAMILY_INTEL_CORE_2_EXTREME_PROCESSOR;
        case 0xC2: return proto::DmiProcessors::FAMILY_INTEL_CORE_2_QUAD_PROCESSOR;
        case 0xC3: return proto::DmiProcessors::FAMILY_INTEL_CORE_2_EXTREME_MOBILE_PROCESSOR;
        case 0xC4: return proto::DmiProcessors::FAMILY_INTEL_CORE_2_DUO_MOBILE_PROCESSOR;
        case 0xC5: return proto::DmiProcessors::FAMILY_INTEL_CORE_2_SOLO_MOBILE_PROCESSOR;
        case 0xC6: return proto::DmiProcessors::FAMILY_INTEL_CORE_I7_PROCESSOR;
        case 0xC7: return proto::DmiProcessors::FAMILY_INTEL_CELERON_DUAL_CORE_PROCESSOR;
        case 0xC8: return proto::DmiProcessors::FAMILY_IBM390_FAMILY;
        case 0xC9: return proto::DmiProcessors::FAMILY_G4;
        case 0xCA: return proto::DmiProcessors::FAMILY_G5;
        case 0xCB: return proto::DmiProcessors::FAMILY_ESA_390_G6;
        case 0xCC: return proto::DmiProcessors::FAMILY_Z_ARCHITECTURE_BASE;
        case 0xCD: return proto::DmiProcessors::FAMILY_INTEL_CORE_I5_PROCESSOR;
        case 0xCE: return proto::DmiProcessors::FAMILY_INTEL_CORE_I3_PROCESSOR;

        case 0xD2: return proto::DmiProcessors::FAMILY_VIA_C7_M_PROCESSOR_FAMILY;
        case 0xD3: return proto::DmiProcessors::FAMILY_VIA_C7_D_PROCESSOR_FAMILY;
        case 0xD4: return proto::DmiProcessors::FAMILY_VIA_C7_PROCESSOR_FAMILY;
        case 0xD5: return proto::DmiProcessors::FAMILY_VIA_EDEN_PROCESSOR_FAMILY;
        case 0xD6: return proto::DmiProcessors::FAMILY_INTEL_XEON_MULTI_CORE_PROCESSOR;
        case 0xD7: return proto::DmiProcessors::FAMILY_INTEL_XEON_DUAL_CORE_3XXX_PROCESSOR_SERIES;
        case 0xD8: return proto::DmiProcessors::FAMILY_INTEL_XEON_QUAD_CORE_3XXX_PROCESSOR_SERIES;
        case 0xD9: return proto::DmiProcessors::FAMILY_VIA_NANO_PROCESSOR_FAMILY;
        case 0xDA: return proto::DmiProcessors::FAMILY_INTEL_XEON_DUAL_CORE_5XXX_PROCESSOR_SERIES;
        case 0xDB: return proto::DmiProcessors::FAMILY_INTEL_XEON_QUAD_CORE_5XXX_PROCESSOR_SERIES;

        case 0xDD: return proto::DmiProcessors::FAMILY_INTEL_XEON_DUAL_CORE_7XXX_PROCESSOR_SERIES;
        case 0xDE: return proto::DmiProcessors::FAMILY_INTEL_XEON_QUAD_CORE_7XXX_PROCESSOR_SERIES;
        case 0xDF: return proto::DmiProcessors::FAMILY_INTEL_XEON_MULTI_CORE_7XXX_PROCESSOR_SERIES;
        case 0xE0: return proto::DmiProcessors::FAMILY_INTEL_XEON_MULTI_CORE_3400_PROCESSOR_SERIES;

        case 0xE4: return proto::DmiProcessors::FAMILY_AMD_OPTERON_3000_PROCESSOR_SERIES;
        case 0xE5: return proto::DmiProcessors::FAMILY_AMD_SEMPRON_II_PROCESSOR;
        case 0xE6: return proto::DmiProcessors::FAMILY_AMD_OPTERON_QUAD_CORE_EMBEDDED_PROCESSOR_FAMILY;
        case 0xE7: return proto::DmiProcessors::FAMILY_AMD_PHENOM_TRIPLE_CORE_PROCESSOR_FAMILY;
        case 0xE8: return proto::DmiProcessors::FAMILY_AMD_TURION_ULTRA_DUAL_CORE_MOBILE_PROCESSOR_FAMILY;
        case 0xE9: return proto::DmiProcessors::FAMILY_AMD_TURION_DUAL_CORE_MOBILE_PROCESSOR_FAMILY;
        case 0xEA: return proto::DmiProcessors::FAMILY_AMD_ATHLON_DUAL_CORE_PROCESSOR_FAMILY;
        case 0xEB: return proto::DmiProcessors::FAMILY_AMD_SEMPRON_SI_PROCESSOR_FAMILY;
        case 0xEC: return proto::DmiProcessors::FAMILY_AMD_PHENOM_2_PROCESSOR_FAMILY;
        case 0xED: return proto::DmiProcessors::FAMILY_AMD_ATHLON_2_PROCESSOR_FAMILY;
        case 0xEE: return proto::DmiProcessors::FAMILY_AMD_OPTERON_SIX_CORE_PROCESSOR_FAMILY;
        case 0xEF: return proto::DmiProcessors::FAMILY_AMD_SEMPRON_M_PROCESSOR_FAMILY;

        case 0xFA: return proto::DmiProcessors::FAMILY_I860;
        case 0xFB: return proto::DmiProcessors::FAMILY_I960;

        case 0x100: return proto::DmiProcessors::FAMILY_ARM_V7;
        case 0x101: return proto::DmiProcessors::FAMILY_ARM_V8;
        case 0x104: return proto::DmiProcessors::FAMILY_SH_3;
        case 0x105: return proto::DmiProcessors::FAMILY_SH_4;
        case 0x118: return proto::DmiProcessors::FAMILY_ARM;
        case 0x119: return proto::DmiProcessors::FAMILY_STRONG_ARM;
        case 0x12C: return proto::DmiProcessors::FAMILY_6X86;
        case 0x12D: return proto::DmiProcessors::FAMILY_MEDIA_GX;
        case 0x12E: return proto::DmiProcessors::FAMILY_MII;
        case 0x140: return proto::DmiProcessors::FAMILY_WIN_CHIP;
        case 0x15E: return proto::DmiProcessors::FAMILY_DSP;
        case 0x1F4: return proto::DmiProcessors::FAMILY_VIDEO_PROCESSOR;

        default:  return proto::DmiProcessors::FAMILY_UNKNOWN;
    }
}

proto::DmiProcessors::Type SMBios::ProcessorTable::GetType() const
{
    if (reader_.GetTableLength() < 0x1A)
        return proto::DmiProcessors::TYPE_UNKNOWN;

    switch (reader_.GetByte(0x05))
    {
        case 0x01: return proto::DmiProcessors::TYPE_OTHER;
        case 0x02: return proto::DmiProcessors::TYPE_UNKNOWN;
        case 0x03: return proto::DmiProcessors::TYPE_CENTRAL_PROCESSOR;
        case 0x04: return proto::DmiProcessors::TYPE_MATH_PROCESSOR;
        case 0x05: return proto::DmiProcessors::TYPE_DSP_PROCESSOR;
        case 0x06: return proto::DmiProcessors::TYPE_VIDEO_PROCESSOR;
        default: return proto::DmiProcessors::TYPE_UNKNOWN;
    }
}

proto::DmiProcessors::Status SMBios::ProcessorTable::GetStatus() const
{
    if (reader_.GetTableLength() < 0x1A)
        return proto::DmiProcessors::STATUS_UNKNOWN;

    switch (BitSet<uint8_t>(reader_.GetByte(0x18)).Range(0, 2))
    {
        case 0x00: return proto::DmiProcessors::STATUS_UNKNOWN;
        case 0x01: return proto::DmiProcessors::STATUS_ENABLED;
        case 0x02: return proto::DmiProcessors::STATUS_DISABLED_BY_USER;
        case 0x03: return proto::DmiProcessors::STATUS_DISABLED_BY_BIOS;
        case 0x04: return proto::DmiProcessors::STATUS_IDLE;
        case 0x07: return proto::DmiProcessors::STATUS_OTHER;
        default: return proto::DmiProcessors::STATUS_UNKNOWN;
    }
}

std::string SMBios::ProcessorTable::GetSocket() const
{
    if (reader_.GetTableLength() < 0x1A)
        return std::string();

    return reader_.GetString(0x04);
}

proto::DmiProcessors::Upgrade SMBios::ProcessorTable::GetUpgrade() const
{
    if (reader_.GetTableLength() < 0x1A)
        return proto::DmiProcessors::UPGRADE_UNKNOWN;

    switch (reader_.GetByte(0x19))
    {
        case 0x01: return proto::DmiProcessors::UPGRADE_OTHER;
        case 0x02: return proto::DmiProcessors::UPGRADE_UNKNOWN;
        case 0x03: return proto::DmiProcessors::UPGRADE_DAUGHTER_BOARD;
        case 0x04: return proto::DmiProcessors::UPGRADE_ZIF_SOCKET;
        case 0x05: return proto::DmiProcessors::UPGRADE_REPLACEABLE_PIGGY_BACK;
        case 0x06: return proto::DmiProcessors::UPGRADE_NONE;
        case 0x07: return proto::DmiProcessors::UPGRADE_LIF_SOCKET;
        case 0x08: return proto::DmiProcessors::UPGRADE_SLOT_1;
        case 0x09: return proto::DmiProcessors::UPGRADE_SLOT_2;
        case 0x0A: return proto::DmiProcessors::UPGRADE_370_PIN_SOCKET;
        case 0x0B: return proto::DmiProcessors::UPGRADE_SLOT_A;
        case 0x0C: return proto::DmiProcessors::UPGRADE_SLOT_M;
        case 0x0D: return proto::DmiProcessors::UPGRADE_SOCKET_423;
        case 0x0E: return proto::DmiProcessors::UPGRADE_SOCKET_462;
        case 0x0F: return proto::DmiProcessors::UPGRADE_SOCKET_478;
        case 0x10: return proto::DmiProcessors::UPGRADE_SOCKET_754;
        case 0x11: return proto::DmiProcessors::UPGRADE_SOCKET_940;
        case 0x12: return proto::DmiProcessors::UPGRADE_SOCKET_939;
        case 0x13: return proto::DmiProcessors::UPGRADE_SOCKET_MPGA604;
        case 0x14: return proto::DmiProcessors::UPGRADE_SOCKET_LGA771;
        case 0x15: return proto::DmiProcessors::UPGRADE_SOCKET_LGA775;
        case 0x16: return proto::DmiProcessors::UPGRADE_SOCKET_S1;
        case 0x17: return proto::DmiProcessors::UPGRADE_SOCKET_AM2;
        case 0x18: return proto::DmiProcessors::UPGRADE_SOCKET_F;
        case 0x19: return proto::DmiProcessors::UPGRADE_SOCKET_LGA1366;
        case 0x1A: return proto::DmiProcessors::UPGRADE_SOCKET_G34;
        case 0x1B: return proto::DmiProcessors::UPGRADE_SOCKET_AM3;
        case 0x1C: return proto::DmiProcessors::UPGRADE_SOCKET_C32;
        case 0x1D: return proto::DmiProcessors::UPGRADE_SOCKET_LGA1156;
        case 0x1E: return proto::DmiProcessors::UPGRADE_SOCKET_LGA1567;
        case 0x1F: return proto::DmiProcessors::UPGRADE_SOCKET_PGA988A;
        case 0x20: return proto::DmiProcessors::UPGRADE_SOCKET_BGA1288;
        case 0x21: return proto::DmiProcessors::UPGRADE_SOCKET_RPGA988B;
        case 0x22: return proto::DmiProcessors::UPGRADE_SOCKET_BGA1023;
        case 0x23: return proto::DmiProcessors::UPGRADE_SOCKET_BGA1224;
        case 0x24: return proto::DmiProcessors::UPGRADE_SOCKET_BGA1155;
        case 0x25: return proto::DmiProcessors::UPGRADE_SOCKET_LGA1356;
        case 0x26: return proto::DmiProcessors::UPGRADE_SOCKET_LGA2011;
        case 0x27: return proto::DmiProcessors::UPGRADE_SOCKET_FS1;
        case 0x28: return proto::DmiProcessors::UPGRADE_SOCKET_FS2;
        case 0x29: return proto::DmiProcessors::UPGRADE_SOCKET_FM1;
        case 0x2A: return proto::DmiProcessors::UPGRADE_SOCKET_FM2;
        case 0x2B: return proto::DmiProcessors::UPGRADE_SOCKET_LGA2011_3;
        case 0x2C: return proto::DmiProcessors::UPGRADE_SOCKET_LGA1356_3;
        case 0x2D: return proto::DmiProcessors::UPGRADE_SOCKET_LGA1150;
        case 0x2E: return proto::DmiProcessors::UPGRADE_SOCKET_BGA1168;
        case 0x2F: return proto::DmiProcessors::UPGRADE_SOCKET_BGA1234;
        case 0x30: return proto::DmiProcessors::UPGRADE_SOCKET_BGA1364;
        case 0x31: return proto::DmiProcessors::UPGRADE_SOCKET_AM4;
        case 0x32: return proto::DmiProcessors::UPGRADE_SOCKET_LGA1151;
        case 0x33: return proto::DmiProcessors::UPGRADE_SOCKET_BGA1356;
        case 0x34: return proto::DmiProcessors::UPGRADE_SOCKET_BGA1440;
        case 0x35: return proto::DmiProcessors::UPGRADE_SOCKET_BGA1515;
        case 0x36: return proto::DmiProcessors::UPGRADE_SOCKET_LGA3647_1;
        case 0x37: return proto::DmiProcessors::UPGRADE_SOCKET_SP3;
        case 0x38: return proto::DmiProcessors::UPGRADE_SOCKET_SP3_R2;
        default: return proto::DmiProcessors::UPGRADE_UNKNOWN;
    }
}

int SMBios::ProcessorTable::GetExternalClock() const
{
    if (reader_.GetTableLength() < 0x1A)
        return 0;

    return reader_.GetWord(0x12);
}

int SMBios::ProcessorTable::GetCurrentSpeed() const
{
    if (reader_.GetTableLength() < 0x1A)
        return 0;

    return reader_.GetWord(0x16);
}

int SMBios::ProcessorTable::GetMaximumSpeed() const
{
    if (reader_.GetTableLength() < 0x1A)
        return 0;

    return reader_.GetWord(0x14);
}

double SMBios::ProcessorTable::GetVoltage() const
{
    if (reader_.GetTableLength() < 0x1A)
        return 0.0;

    const uint8_t voltage = reader_.GetByte(0x11);
    if (!voltage)
        return 0.0;

    if (voltage & 0x80)
        return double(voltage & 0x7F) / 10.0;

    if (voltage & 0x01)
        return 5.0;

    if (voltage & 0x02)
        return 3.3;

    if (voltage & 0x04)
        return 2.9;

    return 0.0;
}

std::string SMBios::ProcessorTable::GetSerialNumber() const
{
    if (reader_.GetTableLength() < 0x23)
        return std::string();

    return reader_.GetString(0x20);
}

std::string SMBios::ProcessorTable::GetAssetTag() const
{
    if (reader_.GetTableLength() < 0x23)
        return std::string();

    return reader_.GetString(0x21);
}

std::string SMBios::ProcessorTable::GetPartNumber() const
{
    if (reader_.GetTableLength() < 0x23)
        return std::string();

    return reader_.GetString(0x22);
}

int SMBios::ProcessorTable::GetCoreCount() const
{
    if (reader_.GetTableLength() < 0x28)
        return 0;

    return reader_.GetByte(0x23);
}

int SMBios::ProcessorTable::GetCoreEnabled() const
{
    if (reader_.GetTableLength() < 0x28)
        return 0;

    return reader_.GetByte(0x24);
}

int SMBios::ProcessorTable::GetThreadCount() const
{
    if (reader_.GetTableLength() < 0x28)
        return 0;

    return reader_.GetByte(0x25);
}

uint32_t SMBios::ProcessorTable::GetCharacteristics() const
{
    if (reader_.GetTableLength() < 0x28)
        return 0;

    BitSet<uint16_t> bitfield(reader_.GetWord(0x26));
    uint32_t characteristics = 0;

    if (bitfield.Test(2))
        characteristics |= proto::DmiProcessors::CHARACTERISTIC_64BIT_CAPABLE;

    if (bitfield.Test(3))
        characteristics |= proto::DmiProcessors::CHARACTERISTIC_MULTI_CORE;

    if (bitfield.Test(4))
        characteristics |= proto::DmiProcessors::CHARACTERISTIC_HARDWARE_THREAD;

    if (bitfield.Test(5))
        characteristics |= proto::DmiProcessors::CHARACTERISTIC_EXECUTE_PROTECTION;

    if (bitfield.Test(6))
        characteristics |= proto::DmiProcessors::CHARACTERISTIC_ENHANCED_VIRTUALIZATION;

    if (bitfield.Test(7))
        characteristics |= proto::DmiProcessors::CHARACTERISTIC_POWER_CONTROL;

    return characteristics;
}

//
// CacheTable
//

SMBios::CacheTable::CacheTable(const TableReader& reader)
    : reader_(reader)
{
    // Nothing
}

std::string SMBios::CacheTable::GetName() const
{
    return reader_.GetString(0x04);
}

proto::DmiCaches::Location SMBios::CacheTable::GetLocation() const
{
    switch (BitSet<uint16_t>(reader_.GetWord(0x05)).Range(5, 6))
    {
        case 0x00: return proto::DmiCaches::LOCATION_INTERNAL;
        case 0x01: return proto::DmiCaches::LOCATION_EXTERNAL;
        case 0x02: return proto::DmiCaches::LOCATION_RESERVED;
        default: return proto::DmiCaches::LOCATION_UNKNOWN;
    }
}

proto::DmiCaches::Status SMBios::CacheTable::GetStatus() const
{
    if (BitSet<uint16_t>(reader_.GetWord(0x05) & 0x0080).Test(7))
        return proto::DmiCaches::STATUS_ENABLED;

    return proto::DmiCaches::STATUS_DISABLED;
}

proto::DmiCaches::Mode SMBios::CacheTable::GetMode() const
{
    switch (BitSet<uint16_t>(reader_.GetWord(0x05)).Range(8, 9))
    {
        case 0x00: return proto::DmiCaches::MODE_WRITE_THRU;
        case 0x01: return proto::DmiCaches::MODE_WRITE_BACK;
        case 0x02: return proto::DmiCaches::MODE_WRITE_WITH_MEMORY_ADDRESS;
        default: return proto::DmiCaches::MODE_UNKNOWN;
    }
}

int SMBios::CacheTable::GetLevel() const
{
    return BitSet<uint16_t>(reader_.GetWord(0x05)).Range(0, 2) + 1;
}

int SMBios::CacheTable::GetMaximumSize() const
{
    return BitSet<uint16_t>(reader_.GetWord(0x07)).Range(0, 14);
}

int SMBios::CacheTable::GetCurrentSize() const
{
    return BitSet<uint16_t>(reader_.GetWord(0x09)).Range(0, 14);
}

uint32_t SMBios::CacheTable::GetSupportedSRAMTypes() const
{
    BitSet<uint16_t> bitfield(reader_.GetWord(0x0B));

    if (bitfield.None())
        return proto::DmiCaches::SRAM_TYPE_BAD;

    uint32_t types = 0;

    if (bitfield.Test(0))
        types |= proto::DmiCaches::SRAM_TYPE_OTHER;

    if (bitfield.Test(1))
        types |= proto::DmiCaches::SRAM_TYPE_UNKNOWN;

    if (bitfield.Test(2))
        types |= proto::DmiCaches::SRAM_TYPE_NON_BURST;

    if (bitfield.Test(3))
        types |= proto::DmiCaches::SRAM_TYPE_BURST;

    if (bitfield.Test(4))
        types |= proto::DmiCaches::SRAM_TYPE_PIPELINE_BURST;

    if (bitfield.Test(5))
        types |= proto::DmiCaches::SRAM_TYPE_SYNCHRONOUS;

    if (bitfield.Test(6))
        types |= proto::DmiCaches::SRAM_TYPE_ASYNCHRONOUS;

    return types;
}

proto::DmiCaches::SRAMType SMBios::CacheTable::GetCurrentSRAMType() const
{
    BitSet<uint16_t> type(reader_.GetWord(0x0D));

    if (type.Test(0))
        return proto::DmiCaches::SRAM_TYPE_OTHER;

    if (type.Test(1))
        return proto::DmiCaches::SRAM_TYPE_UNKNOWN;

    if (type.Test(2))
        return proto::DmiCaches::SRAM_TYPE_NON_BURST;

    if (type.Test(3))
        return proto::DmiCaches::SRAM_TYPE_BURST;

    if (type.Test(4))
        return proto::DmiCaches::SRAM_TYPE_PIPELINE_BURST;

    if (type.Test(5))
        return proto::DmiCaches::SRAM_TYPE_SYNCHRONOUS;

    if (type.Test(6))
        return proto::DmiCaches::SRAM_TYPE_ASYNCHRONOUS;

    return proto::DmiCaches::SRAM_TYPE_BAD;
}

int SMBios::CacheTable::GetSpeed() const
{
    if (reader_.GetTableLength() < 0x13)
        return 0;

    return reader_.GetByte(0x0F);
}

proto::DmiCaches::ErrorCorrectionType SMBios::CacheTable::GetErrorCorrectionType() const
{
    if (reader_.GetTableLength() < 0x13)
        return proto::DmiCaches::ERROR_CORRECTION_TYPE_UNKNOWN;

    switch (reader_.GetByte(0x10))
    {
        case 0x01: return proto::DmiCaches::ERROR_CORRECTION_TYPE_OTHER;
        case 0x03: return proto::DmiCaches::ERROR_CORRECTION_TYPE_NONE;
        case 0x04: return proto::DmiCaches::ERROR_CORRECTION_TYPE_PARITY;
        case 0x05: return proto::DmiCaches::ERROR_CORRECTION_TYPE_SINGLE_BIT_ECC;
        case 0x06: return proto::DmiCaches::ERROR_CORRECTION_TYPE_MULTI_BIT_ECC;
        default: return proto::DmiCaches::ERROR_CORRECTION_TYPE_UNKNOWN;
    }
}

proto::DmiCaches::Type SMBios::CacheTable::GetType() const
{
    if (reader_.GetTableLength() < 0x13)
        return proto::DmiCaches::TYPE_UNKNOWN;

    switch (reader_.GetByte(0x11))
    {
        case 0x01: return proto::DmiCaches::TYPE_OTHER;
        case 0x03: return proto::DmiCaches::TYPE_INSTRUCTION;
        case 0x04: return proto::DmiCaches::TYPE_DATA;
        case 0x05: return proto::DmiCaches::TYPE_UNIFIED;
        default: return proto::DmiCaches::TYPE_UNKNOWN;
    }
}

proto::DmiCaches::Associativity SMBios::CacheTable::GetAssociativity() const
{
    if (reader_.GetTableLength() < 0x13)
        return proto::DmiCaches::ASSOCIATIVITY_UNKNOWN;

    switch (reader_.GetByte(0x12))
    {
        case 0x01: return proto::DmiCaches::ASSOCIATIVITY_OTHER;
        case 0x03: return proto::DmiCaches::ASSOCIATIVITY_DIRECT_MAPPED;
        case 0x04: return proto::DmiCaches::ASSOCIATIVITY_2_WAY;
        case 0x05: return proto::DmiCaches::ASSOCIATIVITY_4_WAY;
        case 0x06: return proto::DmiCaches::ASSOCIATIVITY_FULLY;
        case 0x07: return proto::DmiCaches::ASSOCIATIVITY_8_WAY;
        case 0x08: return proto::DmiCaches::ASSOCIATIVITY_16_WAY;
        case 0x09: return proto::DmiCaches::ASSOCIATIVITY_12_WAY;
        case 0x0A: return proto::DmiCaches::ASSOCIATIVITY_24_WAY;
        case 0x0B: return proto::DmiCaches::ASSOCIATIVITY_32_WAY;
        case 0x0C: return proto::DmiCaches::ASSOCIATIVITY_48_WAY;
        case 0x0D: return proto::DmiCaches::ASSOCIATIVITY_64_WAY;
        case 0x0E: return proto::DmiCaches::ASSOCIATIVITY_20_WAY;
        default: return proto::DmiCaches::ASSOCIATIVITY_UNKNOWN;
    }
}

//
// PortConnectorTable
//

SMBios::PortConnectorTable::PortConnectorTable(const TableReader& reader)
    : reader_(reader)
{
    // Nothing
}

std::string SMBios::PortConnectorTable::GetInternalDesignation() const
{
    if (reader_.GetTableLength() < 0x09)
        return std::string();

    return reader_.GetString(0x04);
}

std::string SMBios::PortConnectorTable::GetExternalDesignation() const
{
    if (reader_.GetTableLength() < 0x09)
        return std::string();

    return reader_.GetString(0x06);
}

proto::DmiPortConnectors::Type SMBios::PortConnectorTable::GetType() const
{
    if (reader_.GetTableLength() < 0x09)
        return proto::DmiPortConnectors::TYPE_UNKNOWN;

    switch (reader_.GetByte(0x08))
    {
        case 0x00: return proto::DmiPortConnectors::TYPE_NONE;
        case 0x01: return proto::DmiPortConnectors::TYPE_PARALLEL_PORT_XT_AT_COMPATIBLE;
        case 0x02: return proto::DmiPortConnectors::TYPE_PARALLEL_PORT_PS_2;
        case 0x03: return proto::DmiPortConnectors::TYPE_PARALLEL_PORT_ECP;
        case 0x04: return proto::DmiPortConnectors::TYPE_PARALLEL_PORT_EPP;
        case 0x05: return proto::DmiPortConnectors::TYPE_PARALLEL_PORT_ECP_EPP;
        case 0x06: return proto::DmiPortConnectors::TYPE_SERIAL_PORT_XT_AT_COMPATIBLE;
        case 0x07: return proto::DmiPortConnectors::TYPE_SERIAL_PORT_16450_COMPATIBLE;
        case 0x08: return proto::DmiPortConnectors::TYPE_SERIAL_PORT_16550_COMPATIBLE;
        case 0x09: return proto::DmiPortConnectors::TYPE_SERIAL_PORT_16550A_COMPATIBLE;
        case 0x0A: return proto::DmiPortConnectors::TYPE_SCSI_PORT;
        case 0x0B: return proto::DmiPortConnectors::TYPE_MIDI_PORT;
        case 0x0C: return proto::DmiPortConnectors::TYPE_JOYSTICK_PORT;
        case 0x0D: return proto::DmiPortConnectors::TYPE_KEYBOARD_PORT;
        case 0x0E: return proto::DmiPortConnectors::TYPE_MOUSE_PORT;
        case 0x0F: return proto::DmiPortConnectors::TYPE_SSA_SCSI;
        case 0x10: return proto::DmiPortConnectors::TYPE_USB;
        case 0x11: return proto::DmiPortConnectors::TYPE_FIREWIRE;
        case 0x12: return proto::DmiPortConnectors::TYPE_PCMCIA_TYPE_I;
        case 0x13: return proto::DmiPortConnectors::TYPE_PCMCIA_TYPE_II;
        case 0x14: return proto::DmiPortConnectors::TYPE_PCMCIA_TYPE_III;
        case 0x15: return proto::DmiPortConnectors::TYPE_CARDBUS;
        case 0x16: return proto::DmiPortConnectors::TYPE_ACCESS_BUS_PORT;
        case 0x17: return proto::DmiPortConnectors::TYPE_SCSI_II;
        case 0x18: return proto::DmiPortConnectors::TYPE_SCSI_WIDE;
        case 0x19: return proto::DmiPortConnectors::TYPE_PC_98;
        case 0x1A: return proto::DmiPortConnectors::TYPE_PC_98_HIRESO;
        case 0x1B: return proto::DmiPortConnectors::TYPE_PC_H98;
        case 0x1C: return proto::DmiPortConnectors::TYPE_VIDEO_PORT;
        case 0x1D: return proto::DmiPortConnectors::TYPE_AUDIO_PORT;
        case 0x1E: return proto::DmiPortConnectors::TYPE_MODEM_PORT;
        case 0x1F: return proto::DmiPortConnectors::TYPE_NETWORK_PORT;
        case 0x20: return proto::DmiPortConnectors::TYPE_SATA;
        case 0x21: return proto::DmiPortConnectors::TYPE_SAS;

        case 0xA0: return proto::DmiPortConnectors::TYPE_8251_COMPATIBLE;
        case 0xA1: return proto::DmiPortConnectors::TYPE_8251_FIFO_COMPATIBLE;

        default: return proto::DmiPortConnectors::TYPE_UNKNOWN;
    }
}

// static
proto::DmiPortConnectors::ConnectorType SMBios::PortConnectorTable::ConnectorType(uint8_t type)
{
    switch (type)
    {
        case 0x00: return proto::DmiPortConnectors::CONNECTOR_TYPE_NONE;
        case 0x01: return proto::DmiPortConnectors::CONNECTOR_TYPE_CENTRONICS;
        case 0x02: return proto::DmiPortConnectors::CONNECTOR_TYPE_MINI_CENTRONICS;
        case 0x03: return proto::DmiPortConnectors::CONNECTOR_TYPE_PROPRIETARY;
        case 0x04: return proto::DmiPortConnectors::CONNECTOR_TYPE_DB_25_MALE;
        case 0x05: return proto::DmiPortConnectors::CONNECTOR_TYPE_DB_25_FEMALE;
        case 0x06: return proto::DmiPortConnectors::CONNECTOR_TYPE_DB_15_MALE;
        case 0x07: return proto::DmiPortConnectors::CONNECTOR_TYPE_DB_15_FEMALE;
        case 0x08: return proto::DmiPortConnectors::CONNECTOR_TYPE_DB_9_MALE;
        case 0x09: return proto::DmiPortConnectors::CONNECTOR_TYPE_DB_9_FEMALE;
        case 0x0A: return proto::DmiPortConnectors::CONNECTOR_TYPE_RJ_11;
        case 0x0B: return proto::DmiPortConnectors::CONNECTOR_TYPE_RJ_45;
        case 0x0C: return proto::DmiPortConnectors::CONNECTOR_TYPE_50_PIN_MINISCSI;
        case 0x0D: return proto::DmiPortConnectors::CONNECTOR_TYPE_MINI_DIN;
        case 0x0E: return proto::DmiPortConnectors::CONNECTOR_TYPE_MICRO_DIN;
        case 0x0F: return proto::DmiPortConnectors::CONNECTOR_TYPE_PS_2;
        case 0x10: return proto::DmiPortConnectors::CONNECTOR_TYPE_INFRARED;
        case 0x11: return proto::DmiPortConnectors::CONNECTOR_TYPE_HP_HIL;
        case 0x12: return proto::DmiPortConnectors::CONNECTOR_TYPE_ACCESS_BUS_USB;
        case 0x13: return proto::DmiPortConnectors::CONNECTOR_TYPE_SSA_SCSI;
        case 0x14: return proto::DmiPortConnectors::CONNECTOR_TYPE_CIRCULAR_DIN_8_MALE;
        case 0x15: return proto::DmiPortConnectors::CONNECTOR_TYPE_CIRCULAR_DIN_8_FEMALE;
        case 0x16: return proto::DmiPortConnectors::CONNECTOR_TYPE_ONBOARD_IDE;
        case 0x17: return proto::DmiPortConnectors::CONNECTOR_TYPE_ONBOARD_FLOPPY;
        case 0x18: return proto::DmiPortConnectors::CONNECTOR_TYPE_9_PIN_DUAL_INLINE;
        case 0x19: return proto::DmiPortConnectors::CONNECTOR_TYPE_25_PIN_DUAL_INLINE;
        case 0x1A: return proto::DmiPortConnectors::CONNECTOR_TYPE_50_PIN_DUAL_INLINE;
        case 0x1B: return proto::DmiPortConnectors::CONNECTOR_TYPE_68_PIN_DUAL_INLINE;
        case 0x1C: return proto::DmiPortConnectors::CONNECTOR_TYPE_ONBOARD_SOUND_INPUT_FROM_CDROM;
        case 0x1D: return proto::DmiPortConnectors::CONNECTOR_TYPE_MINI_CENTRONICS_TYPE_14;
        case 0x1E: return proto::DmiPortConnectors::CONNECTOR_TYPE_MINI_CENTRONICS_TYPE_26;
        case 0x1F: return proto::DmiPortConnectors::CONNECTOR_TYPE_MINI_JACK;
        case 0x20: return proto::DmiPortConnectors::CONNECTOR_TYPE_BNC;
        case 0x21: return proto::DmiPortConnectors::CONNECTOR_TYPE_IEEE_1394;
        case 0x22: return proto::DmiPortConnectors::CONNECTOR_TYPE_SAS_SATE_PLUG_RECEPTACLE;

        case 0xA0: return proto::DmiPortConnectors::CONNECTOR_TYPE_PC_98;
        case 0xA1: return proto::DmiPortConnectors::CONNECTOR_TYPE_PC_98_HIRESO;
        case 0xA2: return proto::DmiPortConnectors::CONNECTOR_TYPE_PC_H98;
        case 0xA3: return proto::DmiPortConnectors::CONNECTOR_TYPE_PC_98_NOTE;
        case 0xA4: return proto::DmiPortConnectors::CONNECTOR_TYPE_PC_98_FULL;

        case 0xFF: return proto::DmiPortConnectors::CONNECTOR_TYPE_OTHER;

        default: return proto::DmiPortConnectors::CONNECTOR_TYPE_UNKNOWN;
    }
}

proto::DmiPortConnectors::ConnectorType
    SMBios::PortConnectorTable::GetInternalConnectorType() const
{
    if (reader_.GetTableLength() < 0x09)
        return proto::DmiPortConnectors::CONNECTOR_TYPE_UNKNOWN;

    return ConnectorType(reader_.GetByte(0x05));
}

proto::DmiPortConnectors::ConnectorType
    SMBios::PortConnectorTable::GetExternalConnectorType() const
{
    if (reader_.GetTableLength() < 0x09)
        return proto::DmiPortConnectors::CONNECTOR_TYPE_UNKNOWN;

    return ConnectorType(reader_.GetByte(0x07));
}

//
// SystemSlotTable
//

SMBios::SystemSlotTable::SystemSlotTable(const TableReader& reader)
    : reader_(reader)
{
    // Nothing
}

std::string SMBios::SystemSlotTable::GetSlotDesignation() const
{
    if (reader_.GetTableLength() < 0x0C)
        return std::string();

    return reader_.GetString(0x04);
}

proto::DmiSystemSlots::Type SMBios::SystemSlotTable::GetType() const
{
    if (reader_.GetTableLength() < 0x0C)
        return proto::DmiSystemSlots::TYPE_UNKNOWN;

    switch (reader_.GetByte(0x05))
    {
        case 0x01: return proto::DmiSystemSlots::TYPE_OTHER;
        case 0x02: return proto::DmiSystemSlots::TYPE_UNKNOWN;
        case 0x03: return proto::DmiSystemSlots::TYPE_ISA;
        case 0x04: return proto::DmiSystemSlots::TYPE_MCA;
        case 0x05: return proto::DmiSystemSlots::TYPE_EISA;
        case 0x06: return proto::DmiSystemSlots::TYPE_PCI;
        case 0x07: return proto::DmiSystemSlots::TYPE_PC_CARD;
        case 0x08: return proto::DmiSystemSlots::TYPE_VLB;
        case 0x09: return proto::DmiSystemSlots::TYPE_PROPRIETARY;
        case 0x0A: return proto::DmiSystemSlots::TYPE_PROCESSOR_CARD;
        case 0x0B: return proto::DmiSystemSlots::TYPE_PROPRIETARY_MEMORY_CARD;
        case 0x0C: return proto::DmiSystemSlots::TYPE_IO_RISER_CARD;
        case 0x0D: return proto::DmiSystemSlots::TYPE_NUBUS;
        case 0x0E: return proto::DmiSystemSlots::TYPE_PCI_66;
        case 0x0F: return proto::DmiSystemSlots::TYPE_AGP;
        case 0x10: return proto::DmiSystemSlots::TYPE_AGP_2X;
        case 0x11: return proto::DmiSystemSlots::TYPE_AGP_4X;
        case 0x12: return proto::DmiSystemSlots::TYPE_PCI_X;
        case 0x13: return proto::DmiSystemSlots::TYPE_AGP_8X;
        case 0x14: return proto::DmiSystemSlots::TYPE_M2_SOCKET_1DP;
        case 0x15: return proto::DmiSystemSlots::TYPE_M2_SOCKET_1SD;
        case 0x16: return proto::DmiSystemSlots::TYPE_M2_SOCKET_2;
        case 0x17: return proto::DmiSystemSlots::TYPE_M2_SOCKET_3;
        case 0x18: return proto::DmiSystemSlots::TYPE_MXM_TYPE_I;
        case 0x19: return proto::DmiSystemSlots::TYPE_MXM_TYPE_II;
        case 0x1A: return proto::DmiSystemSlots::TYPE_MXM_TYPE_III;
        case 0x1B: return proto::DmiSystemSlots::TYPE_MXM_TYPE_III_HE;
        case 0x1C: return proto::DmiSystemSlots::TYPE_MXM_TYPE_IV;
        case 0x1D: return proto::DmiSystemSlots::TYPE_MXM_30_TYPE_A;
        case 0x1E: return proto::DmiSystemSlots::TYPE_MXM_30_TYPE_B;
        case 0x1F: return proto::DmiSystemSlots::TYPE_PCI_EXPRESS_2_SFF_8639;
        case 0x20: return proto::DmiSystemSlots::TYPE_PCI_EXPRESS_3_SFF_8639;
        case 0x21: return proto::DmiSystemSlots::TYPE_PCI_EXPRESS_MINI_52PIN_WITH_BOTTOM_SIDE;
        case 0x22: return proto::DmiSystemSlots::TYPE_PCI_EXPRESS_MINI_52PIN;
        case 0x23: return proto::DmiSystemSlots::TYPE_PCI_EXPRESS_MINI_76PIN;

        case 0xA0: return proto::DmiSystemSlots::TYPE_PC98_C20;
        case 0xA1: return proto::DmiSystemSlots::TYPE_PC98_C24;
        case 0xA2: return proto::DmiSystemSlots::TYPE_PC98_E;
        case 0xA3: return proto::DmiSystemSlots::TYPE_PC98_LOCAL_BUS;
        case 0xA4: return proto::DmiSystemSlots::TYPE_PC98_CARD;
        case 0xA5: return proto::DmiSystemSlots::TYPE_PCI_EXPRESS;
        case 0xA6: return proto::DmiSystemSlots::TYPE_PCI_EXPRESS_X1;
        case 0xA7: return proto::DmiSystemSlots::TYPE_PCI_EXPRESS_X2;
        case 0xA8: return proto::DmiSystemSlots::TYPE_PCI_EXPRESS_X4;
        case 0xA9: return proto::DmiSystemSlots::TYPE_PCI_EXPRESS_X8;
        case 0xAA: return proto::DmiSystemSlots::TYPE_PCI_EXPRESS_X16;
        case 0xAB: return proto::DmiSystemSlots::TYPE_PCI_EXPRESS_2;
        case 0xAC: return proto::DmiSystemSlots::TYPE_PCI_EXPRESS_2_X1;
        case 0xAD: return proto::DmiSystemSlots::TYPE_PCI_EXPRESS_2_X2;
        case 0xAE: return proto::DmiSystemSlots::TYPE_PCI_EXPRESS_2_X4;
        case 0xAF: return proto::DmiSystemSlots::TYPE_PCI_EXPRESS_2_X8;
        case 0xB0: return proto::DmiSystemSlots::TYPE_PCI_EXPRESS_2_X16;
        case 0xB1: return proto::DmiSystemSlots::TYPE_PCI_EXPRESS_3;
        case 0xB2: return proto::DmiSystemSlots::TYPE_PCI_EXPRESS_3_X1;
        case 0xB3: return proto::DmiSystemSlots::TYPE_PCI_EXPRESS_3_X2;
        case 0xB4: return proto::DmiSystemSlots::TYPE_PCI_EXPRESS_3_X4;
        case 0xB5: return proto::DmiSystemSlots::TYPE_PCI_EXPRESS_3_X8;
        case 0xB6: return proto::DmiSystemSlots::TYPE_PCI_EXPRESS_3_X16;

        default: return proto::DmiSystemSlots::TYPE_UNKNOWN;
    }
}

proto::DmiSystemSlots::Usage SMBios::SystemSlotTable::GetUsage() const
{
    if (reader_.GetTableLength() < 0x0C)
        return proto::DmiSystemSlots::USAGE_UNKNOWN;

    switch (reader_.GetByte(0x07))
    {
        case 0x01: return proto::DmiSystemSlots::USAGE_OTHER;
        case 0x02: return proto::DmiSystemSlots::USAGE_UNKNOWN;
        case 0x03: return proto::DmiSystemSlots::USAGE_AVAILABLE;
        case 0x04: return proto::DmiSystemSlots::USAGE_IN_USE;
        default: return proto::DmiSystemSlots::USAGE_UNKNOWN;
    }
}

proto::DmiSystemSlots::BusWidth SMBios::SystemSlotTable::GetBusWidth() const
{
    if (reader_.GetTableLength() < 0x0C)
        return proto::DmiSystemSlots::BUS_WIDTH_UNKNOWN;

    switch (reader_.GetByte(0x06))
    {
        case 0x01: return proto::DmiSystemSlots::BUS_WIDTH_OTHER;
        case 0x02: return proto::DmiSystemSlots::BUS_WIDTH_UNKNOWN;
        case 0x03: return proto::DmiSystemSlots::BUS_WIDTH_8_BIT;
        case 0x04: return proto::DmiSystemSlots::BUS_WIDTH_16_BIT;
        case 0x05: return proto::DmiSystemSlots::BUS_WIDTH_32_BIT;
        case 0x06: return proto::DmiSystemSlots::BUS_WIDTH_64_BIT;
        case 0x07: return proto::DmiSystemSlots::BUS_WIDTH_128_BIT;
        case 0x08: return proto::DmiSystemSlots::BUS_WIDTH_X1;
        case 0x09: return proto::DmiSystemSlots::BUS_WIDTH_X2;
        case 0x0A: return proto::DmiSystemSlots::BUS_WIDTH_X4;
        case 0x0B: return proto::DmiSystemSlots::BUS_WIDTH_X8;
        case 0x0C: return proto::DmiSystemSlots::BUS_WIDTH_X12;
        case 0x0D: return proto::DmiSystemSlots::BUS_WIDTH_X16;
        case 0x0E: return proto::DmiSystemSlots::BUS_WIDTH_X32;
        default: return proto::DmiSystemSlots::BUS_WIDTH_OTHER;
    }
}

proto::DmiSystemSlots::Length SMBios::SystemSlotTable::GetLength() const
{
    if (reader_.GetTableLength() < 0x0C)
        return proto::DmiSystemSlots::LENGTH_UNKNOWN;

    switch (reader_.GetByte(0x08))
    {
        case 0x01: return proto::DmiSystemSlots::LENGTH_OTHER;
        case 0x02: return proto::DmiSystemSlots::LENGTH_UNKNOWN;
        case 0x03: return proto::DmiSystemSlots::LENGTH_SHORT;
        case 0x04: return proto::DmiSystemSlots::LENGTH_LONG;
        default: return proto::DmiSystemSlots::LENGTH_UNKNOWN;
    }
}

//
// OnBoardDeviceTable
//

SMBios::OnBoardDeviceTable::OnBoardDeviceTable(const TableReader& reader)
    : count_((reader.GetTableLength() - 4) / 2),
      ptr_(reader.GetPointer(0) + 4),
      reader_(reader)
{
    // Nothing
}

int SMBios::OnBoardDeviceTable::GetDeviceCount() const
{
    return count_;
}

std::string SMBios::OnBoardDeviceTable::GetDescription(int index) const
{
    DCHECK(index < count_);

    uint8_t handle = ptr_[2 * index + 1];
    if (!handle)
        return std::string();

    char* string = reinterpret_cast<char*>(const_cast<uint8_t*>(
        reader_.GetPointer(0))) + reader_.GetTableLength();

    while (handle > 1 && *string)
    {
        string += strlen(string);
        ++string;
        --handle;
    }

    if (!*string || !string[0])
        return std::string();

    std::string output;
    TrimWhitespaceASCII(string, TRIM_ALL, output);
    return output;
}

proto::DmiOnBoardDevices::Type SMBios::OnBoardDeviceTable::GetType(int index) const
{
    DCHECK(index < count_);

    switch (BitSet<uint8_t>(ptr_[2 * index]).Range(0, 6))
    {
        case 0x01: return proto::DmiOnBoardDevices::TYPE_OTHER;
        case 0x02: return proto::DmiOnBoardDevices::TYPE_UNKNOWN;
        case 0x03: return proto::DmiOnBoardDevices::TYPE_VIDEO;
        case 0x04: return proto::DmiOnBoardDevices::TYPE_SCSI_CONTROLLER;
        case 0x05: return proto::DmiOnBoardDevices::TYPE_ETHERNET;
        case 0x06: return proto::DmiOnBoardDevices::TYPE_TOKEN_RING;
        case 0x07: return proto::DmiOnBoardDevices::TYPE_SOUND;
        case 0x08: return proto::DmiOnBoardDevices::TYPE_PATA_CONTROLLER;
        case 0x09: return proto::DmiOnBoardDevices::TYPE_SATA_CONTROLLER;
        case 0x0A: return proto::DmiOnBoardDevices::TYPE_SAS_CONTROLLER;
        default: return proto::DmiOnBoardDevices::TYPE_UNKNOWN;
    }
}

bool SMBios::OnBoardDeviceTable::IsEnabled(int index) const
{
    DCHECK(index < count_);
    return BitSet<uint8_t>(ptr_[2 * index]).Test(7);
}

//
// MemoryDeviceTable
//

SMBios::MemoryDeviceTable::MemoryDeviceTable(const TableReader& reader)
    : reader_(reader)
{
    // Nothing
}

std::string SMBios::MemoryDeviceTable::GetDeviceLocator() const
{
    if (reader_.GetTableLength() < 0x15)
        return std::string();

    return reader_.GetString(0x10);
}

int SMBios::MemoryDeviceTable::GetSize() const
{
    if (reader_.GetTableLength() < 0x15)
        return 0;

    return reader_.GetWord(0x0C) & 0x07FFF;
}

proto::DmiMemoryDevices::Type SMBios::MemoryDeviceTable::GetType() const
{
    if (reader_.GetTableLength() < 0x15)
        return proto::DmiMemoryDevices::TYPE_UNKNOWN;

    switch (reader_.GetByte(0x12))
    {
        case 0x01: return proto::DmiMemoryDevices::TYPE_OTHER;
        case 0x02: return proto::DmiMemoryDevices::TYPE_UNKNOWN;
        case 0x03: return proto::DmiMemoryDevices::TYPE_DRAM;
        case 0x04: return proto::DmiMemoryDevices::TYPE_EDRAM;
        case 0x05: return proto::DmiMemoryDevices::TYPE_VRAM;
        case 0x06: return proto::DmiMemoryDevices::TYPE_SRAM;
        case 0x07: return proto::DmiMemoryDevices::TYPE_RAM;
        case 0x08: return proto::DmiMemoryDevices::TYPE_ROM;
        case 0x09: return proto::DmiMemoryDevices::TYPE_FLASH;
        case 0x0A: return proto::DmiMemoryDevices::TYPE_EEPROM;
        case 0x0B: return proto::DmiMemoryDevices::TYPE_FEPROM;
        case 0x0C: return proto::DmiMemoryDevices::TYPE_EPROM;
        case 0x0D: return proto::DmiMemoryDevices::TYPE_CDRAM;
        case 0x0E: return proto::DmiMemoryDevices::TYPE_3DRAM;
        case 0x0F: return proto::DmiMemoryDevices::TYPE_SDRAM;
        case 0x10: return proto::DmiMemoryDevices::TYPE_SGRAM;
        case 0x11: return proto::DmiMemoryDevices::TYPE_RDRAM;
        case 0x12: return proto::DmiMemoryDevices::TYPE_DDR;
        case 0x13: return proto::DmiMemoryDevices::TYPE_DDR2;
        case 0x14: return proto::DmiMemoryDevices::TYPE_DDR2_FB_DIMM;

        case 0x18: return proto::DmiMemoryDevices::TYPE_DDR3;
        case 0x19: return proto::DmiMemoryDevices::TYPE_FBD2;
        case 0x1A: return proto::DmiMemoryDevices::TYPE_DDR4;
        case 0x1B: return proto::DmiMemoryDevices::TYPE_LPDDR;
        case 0x1C: return proto::DmiMemoryDevices::TYPE_LPDDR2;
        case 0x1D: return proto::DmiMemoryDevices::TYPE_LPDDR3;
        case 0x1E: return proto::DmiMemoryDevices::TYPE_LPDDR4;

        default: return proto::DmiMemoryDevices::TYPE_UNKNOWN;
    }
}

int SMBios::MemoryDeviceTable::GetSpeed() const
{
    if (reader_.GetTableLength() < 0x15)
        return 0;

    return reader_.GetWord(0x15);
}

proto::DmiMemoryDevices::FormFactor SMBios::MemoryDeviceTable::GetFormFactor() const
{
    if (reader_.GetTableLength() < 0x15)
        return proto::DmiMemoryDevices::FORM_FACTOR_UNKNOWN;

    switch (reader_.GetByte(0x0E))
    {
        case 0x01: return proto::DmiMemoryDevices::FORM_FACTOR_OTHER;
        case 0x02: return proto::DmiMemoryDevices::FORM_FACTOR_UNKNOWN;
        case 0x03: return proto::DmiMemoryDevices::FORM_FACTOR_SIMM;
        case 0x04: return proto::DmiMemoryDevices::FORM_FACTOR_SIP;
        case 0x05: return proto::DmiMemoryDevices::FORM_FACTOR_CHIP;
        case 0x06: return proto::DmiMemoryDevices::FORM_FACTOR_DIP;
        case 0x07: return proto::DmiMemoryDevices::FORM_FACTOR_ZIP;
        case 0x08: return proto::DmiMemoryDevices::FORM_FACTOR_PROPRIETARY_CARD;
        case 0x09: return proto::DmiMemoryDevices::FORM_FACTOR_DIMM;
        case 0x0A: return proto::DmiMemoryDevices::FORM_FACTOR_TSOP;
        case 0x0B: return proto::DmiMemoryDevices::FORM_FACTOR_ROW_OF_CHIPS;
        case 0x0C: return proto::DmiMemoryDevices::FORM_FACTOR_RIMM;
        case 0x0D: return proto::DmiMemoryDevices::FORM_FACTOR_SODIMM;
        case 0x0E: return proto::DmiMemoryDevices::FORM_FACTOR_SRIMM;
        case 0x0F: return proto::DmiMemoryDevices::FORM_FACTOR_FB_DIMM;
        default: return proto::DmiMemoryDevices::FORM_FACTOR_UNKNOWN;
    }
}

std::string SMBios::MemoryDeviceTable::GetSerialNumber() const
{
    if (reader_.GetTableLength() < 0x15)
        return std::string();

    return reader_.GetString(0x18);
}

std::string SMBios::MemoryDeviceTable::GetPartNumber() const
{
    if (reader_.GetTableLength() < 0x15)
        return std::string();

    return reader_.GetString(0x1A);
}

std::string SMBios::MemoryDeviceTable::GetManufacturer() const
{
    if (reader_.GetTableLength() < 0x15)
        return std::string();

    return reader_.GetString(0x17);
}

std::string SMBios::MemoryDeviceTable::GetBank() const
{
    if (reader_.GetTableLength() < 0x15)
        return std::string();

    return reader_.GetString(0x11);
}

int SMBios::MemoryDeviceTable::GetTotalWidth() const
{
    if (reader_.GetTableLength() < 0x15)
        return 0;

    return reader_.GetWord(0x08);
}

int SMBios::MemoryDeviceTable::GetDataWidth() const
{
    if (reader_.GetTableLength() < 0x15)
        return 0;

    return reader_.GetWord(0x0A);
}

//
// PointingDeviceTable
//

SMBios::PointingDeviceTable::PointingDeviceTable(const TableReader& reader)
    : reader_(reader)
{
    // Nothing
}

proto::DmiPointingDevices::Type SMBios::PointingDeviceTable::GetDeviceType() const
{
    if (reader_.GetTableLength() < 0x07)
        return proto::DmiPointingDevices::TYPE_UNKNOWN;

    switch (reader_.GetByte(0x04))
    {
        case 0x01: return proto::DmiPointingDevices::TYPE_OTHER;
        case 0x02: return proto::DmiPointingDevices::TYPE_UNKNOWN;
        case 0x03: return proto::DmiPointingDevices::TYPE_MOUSE;
        case 0x04: return proto::DmiPointingDevices::TYPE_TRACK_BALL;
        case 0x05: return proto::DmiPointingDevices::TYPE_TRACK_POINT;
        case 0x06: return proto::DmiPointingDevices::TYPE_GLIDE_POINT;
        case 0x07: return proto::DmiPointingDevices::TYPE_TOUCH_PAD;
        case 0x08: return proto::DmiPointingDevices::TYPE_TOUCH_SCREEN;
        case 0x09: return proto::DmiPointingDevices::TYPE_OPTICAL_SENSOR;
        default: return proto::DmiPointingDevices::TYPE_UNKNOWN;
    }
}

proto::DmiPointingDevices::Interface SMBios::PointingDeviceTable::GetInterface() const
{
    if (reader_.GetTableLength() < 0x07)
        return proto::DmiPointingDevices::INTERFACE_UNKNOWN;

    switch (reader_.GetByte(0x05))
    {
        case 0x01: return proto::DmiPointingDevices::INTERFACE_OTHER;
        case 0x02: return proto::DmiPointingDevices::INTERFACE_UNKNOWN;
        case 0x03: return proto::DmiPointingDevices::INTERFACE_SERIAL;
        case 0x04: return proto::DmiPointingDevices::INTERFACE_PS_2;
        case 0x05: return proto::DmiPointingDevices::INTERFACE_INFRARED;
        case 0x06: return proto::DmiPointingDevices::INTERFACE_HP_HIL;
        case 0x07: return proto::DmiPointingDevices::INTERFACE_BUS_MOUSE;
        case 0x08: return proto::DmiPointingDevices::INTERFACE_ADB;
        case 0xA0: return proto::DmiPointingDevices::INTERFACE_BUS_MOUSE_DB_9;
        case 0xA1: return proto::DmiPointingDevices::INTERFACE_BUS_MOUSE_MICRO_DIN;
        case 0xA2: return proto::DmiPointingDevices::INTERFACE_USB;
        default: return proto::DmiPointingDevices::INTERFACE_UNKNOWN;
    }
}

int SMBios::PointingDeviceTable::GetButtonCount() const
{
    if (reader_.GetTableLength() < 0x07)
        return 0;

    return reader_.GetByte(0x06);
}

//
// PortableBatteryTable
//

SMBios::PortableBatteryTable::PortableBatteryTable(const TableReader& reader)
    : reader_(reader)
{
    // Nothing
}

std::string SMBios::PortableBatteryTable::GetLocation() const
{
    if (reader_.GetTableLength() < 0x10)
        return std::string();

    return reader_.GetString(0x04);
}

std::string SMBios::PortableBatteryTable::GetManufacturer() const
{
    if (reader_.GetTableLength() < 0x10)
        return std::string();

    return reader_.GetString(0x05);
}

std::string SMBios::PortableBatteryTable::GetManufactureDate() const
{
    if (reader_.GetTableLength() < 0x10)
        return std::string();

    return reader_.GetString(0x06);
}

std::string SMBios::PortableBatteryTable::GetSerialNumber() const
{
    if (reader_.GetTableLength() < 0x10)
        return std::string();

    return reader_.GetString(0x07);
}

std::string SMBios::PortableBatteryTable::GetDeviceName() const
{
    if (reader_.GetTableLength() < 0x10)
        return std::string();

    return reader_.GetString(0x08);
}

proto::DmiPortableBattery::Chemistry SMBios::PortableBatteryTable::GetChemistry() const
{
    if (reader_.GetTableLength() < 0x10)
        return proto::DmiPortableBattery::CHEMISTRY_UNKNOWN;

    switch (reader_.GetByte(0x09))
    {
        case 0x01: return proto::DmiPortableBattery::CHEMISTRY_OTHER;
        case 0x02: return proto::DmiPortableBattery::CHEMISTRY_UNKNOWN;
        case 0x03: return proto::DmiPortableBattery::CHEMISTRY_LEAD_ACID;
        case 0x04: return proto::DmiPortableBattery::CHEMISTRY_NICKEL_CADMIUM;
        case 0x05: return proto::DmiPortableBattery::CHEMISTRY_NICKEL_METAL_HYDRIDE;
        case 0x06: return proto::DmiPortableBattery::CHEMISTRY_LITHIUM_ION;
        case 0x07: return proto::DmiPortableBattery::CHEMISTRY_ZINC_AIR;
        case 0x08: return proto::DmiPortableBattery::CHEMISTRY_LITHIUM_POLYMER;
        default: return proto::DmiPortableBattery::CHEMISTRY_UNKNOWN;
    }
}

int SMBios::PortableBatteryTable::GetDesignCapacity() const
{
    const uint8_t length = reader_.GetTableLength();
    if (length < 0x10)
        return 0;

    if (length < 0x16)
        return reader_.GetWord(0x0A);

    return reader_.GetWord(0x0A) + reader_.GetByte(0x15);
}

int SMBios::PortableBatteryTable::GetDesignVoltage() const
{
    if (reader_.GetTableLength() < 0x10)
        return 0;

    return reader_.GetWord(0x0C);
}

std::string SMBios::PortableBatteryTable::GetSBDSVersionNumber() const
{
    if (reader_.GetTableLength() < 0x10)
        return std::string();

    return reader_.GetString(0x0E);
}

int SMBios::PortableBatteryTable::GetMaxErrorInBatteryData() const
{
    if (reader_.GetTableLength() < 0x10)
        return 0;

    return reader_.GetByte(0x0F);
}

std::string SMBios::PortableBatteryTable::GetSBDSSerialNumber() const
{
    if (reader_.GetTableLength() < 0x1A)
        return std::string();

    return StringPrintf("%04X", reader_.GetWord(0x10));
}

std::string SMBios::PortableBatteryTable::GetSBDSManufactureDate() const
{
    if (reader_.GetTableLength() < 0x1A)
        return std::string();

    const BitSet<uint16_t> date(reader_.GetWord(0x12));

    return StringPrintf("%02u/%02u/%u",
                        date.Range(0, 4), // Day.
                        date.Range(5, 8), // Month.
                        1980U + date.Range(9, 15)); // Year.
}

std::string SMBios::PortableBatteryTable::GetSBDSDeviceChemistry() const
{
    if (reader_.GetTableLength() < 0x1A)
        return std::string();

    return reader_.GetString(0x14);
}

} // namespace aspia
