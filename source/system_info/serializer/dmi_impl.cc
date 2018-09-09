//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "system_info/serializer/dmi_impl.h"

#include <QString>

#include "base/bitset.h"
#include "base/logging.h"
#include "build_config.h"

#if defined(OS_WIN)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace aspia {

namespace {

bool readSmBios(void* data, size_t data_size)
{
#if defined(OS_WIN)
    if (!GetSystemFirmwareTable('RSMB', 'PCAF', data, data_size))
    {
        PLOG(LS_WARNING) << "GetSystemFirmwareTable failed";
        return false;
    }

    return true;
#else
#error Platform support not implemented
#endif
}

} // namespace

//================================================================================================
// DmiTableEnumerator implementation.
//================================================================================================

DmiTableEnumerator::DmiTableEnumerator()
{
    if (!readSmBios(&data_, sizeof(SmBiosData)))
        return;

    current_ = next_ = &data_.smbios_table_data[0];
}

void DmiTableEnumerator::advance()
{
    current_ = next_;

    const uint8_t* start = &data_.smbios_table_data[0];
    const uint8_t* end = start + data_.length;

    while (current_ + 4 <= end)
    {
        const uint8_t table_type = current_[0];
        const uint8_t table_length = current_[1];

        // If a short entry is found (less than 4 bytes), not only it is invalid, but we
        // cannot reliably locate the next entry. Better stop at this point, and let the
        // user know his / her table is broken.
        if (table_length < 4)
        {
            LOG(LS_WARNING) << "Invalid SMBIOS table length";
            break;
        }

        // Stop decoding at end of table marker.
        if (table_type == 127)
            break;

        // Look for the next handle.
        next_ = current_ + table_length;
        while (static_cast<quintptr>(next_ - start + 1) < data_.length &&
            (next_[0] != 0 || next_[1] != 0))
        {
            ++next_;
        }

        // Points to the next table thas after two null bytes at the end of the strings.
        next_ += 2;
        return;
    }

    current_ = nullptr;
    next_ = nullptr;
}

const DmiTable* DmiTableEnumerator::table() const
{
    if (!current_)
        return nullptr;

    switch (current_[0])
    {
        case DmiTable::TYPE_BIOS:
            current_table_.reset(new DmiBiosTable(current_));
            break;

        default:
            current_table_.reset();
            break;
    }

    return current_table_.get();
}

//================================================================================================
// DmiTable implementation.
//================================================================================================

DmiTable::DmiTable(const uint8_t* table)
    : table_(table)
{
    // Nothing
}

QString DmiTable::string(uint8_t offset) const
{
    uint8_t handle = table_[offset];
    if (!handle)
        return QString();

    char* string = reinterpret_cast<char*>(const_cast<uint8_t*>(&table_[0])) + length();
    while (handle > 1 && *string)
    {
        string += strlen(string) + 1;
        --handle;
    }

    if (!*string)
        return QString();

    return QLatin1String(string).trimmed();
}

//================================================================================================
// DmiBiosTable implementation.
//================================================================================================

DmiBiosTable::DmiBiosTable(const uint8_t* table)
    : DmiTable(table)
{
    // Nothing
}

QString DmiBiosTable::manufacturer() const
{
    return string(0x04);
}

QString DmiBiosTable::version() const
{
    return string(0x05);
}

QString DmiBiosTable::date() const
{
    return string(0x08);
}

quint64 DmiBiosTable::biosSize() const
{
    const uint8_t old_size = number<uint8_t>(0x09);
    if (old_size != 0xFF)
        return (old_size + 1) << 6;

    BitSet<uint16_t> bitfield(number<uint16_t>(0x18));

    uint16_t size = 16; // By default 16 MBytes.

    if (length() >= 0x1A)
        size = bitfield.range(0, 13);

    switch (bitfield.range(14, 15))
    {
        case 0x0000: // MB
            return static_cast<quint64>(size) * 1024ULL;

        case 0x0001: // GB
            return static_cast<quint64>(size) * 1024ULL * 1024ULL;

        default:
            return 0;
    }
}

QString DmiBiosTable::biosRevision() const
{
    const uint8_t major = number<uint8_t>(0x14);
    const uint8_t minor = number<uint8_t>(0x15);

    if (major == 0xFF || minor == 0xFF)
        return QString();

    return QString("%1.%2").arg(major).arg(minor);
}

QString DmiBiosTable::firmwareRevision() const
{
    const uint8_t major = number<uint8_t>(0x16);
    const uint8_t minor = number<uint8_t>(0x17);

    if (major == 0xFF || minor == 0xFF)
        return QString();

    return QString("%1.%2").arg(major).arg(minor);
}

QString DmiBiosTable::address() const
{
    const uint16_t address = number<uint16_t>(0x06);
    if (!address)
        return QString();

    return QString("%10h").arg(address, 4, 16);
}

quint64 DmiBiosTable::runtimeSize() const
{
    const uint16_t address = number<uint16_t>(0x06);
    if (!address)
        return 0;

    const uint32_t code = (0x10000 - address) << 4;

    if (code & 0x000003FF)
        return code;

    return (code >> 10) * 1024;
}

void DmiBiosTable::characteristics(Characteristics* result) const
{
    memset(result, 0, sizeof(Characteristics));

    BitSet<quint64> bf = number<quint64>(0x0A);
    if (!bf.test(3))
    {
        result->isa                         = bf.test(4);
        result->mca                         = bf.test(5);
        result->eisa                        = bf.test(6);
        result->pci                         = bf.test(7);
        result->pc_card                     = bf.test(8);
        result->pnp                         = bf.test(9);
        result->apm                         = bf.test(10);
        result->bios_upgradeable            = bf.test(11);
        result->bios_shadowing              = bf.test(12);
        result->vlb                         = bf.test(13);
        result->escd                        = bf.test(14);
        result->boot_from_cd                = bf.test(15);
        result->selectable_boot             = bf.test(16);
        result->socketed_boot_rom           = bf.test(17);
        result->boot_from_pc_card           = bf.test(18);
        result->edd                         = bf.test(19);
        result->japanese_floppy_for_nec9800 = bf.test(20);
        result->japanese_floppy_for_toshiba = bf.test(21);
        result->floppy_525_360kb            = bf.test(22);
        result->floppy_525_12mb             = bf.test(23);
        result->floppy_35_720kb             = bf.test(24);
        result->floppy_35_288mb             = bf.test(25);
        result->print_screen                = bf.test(26);
        result->keyboard_8042               = bf.test(27);
        result->serial                      = bf.test(28);
        result->printer                     = bf.test(29);
        result->cga_video                   = bf.test(30);
        result->nec_pc98                    = bf.test(31);
    }

    if (length() >= 0x13)
    {
        BitSet<uint8_t> bf1 = number<uint8_t>(0x12);

        result->acpi                 = bf1.test(0);
        result->usb_legacy           = bf1.test(1);
        result->agp                  = bf1.test(2);
        result->i2o_boot             = bf1.test(3);
        result->ls120_boot           = bf1.test(4);
        result->atapi_zip_drive_boot = bf1.test(5);
        result->ieee1394_boot        = bf1.test(6);
        result->smart_battery        = bf1.test(7);
    }

    if (length() >= 0x14)
    {
        BitSet<uint8_t> bf2 = number<uint8_t>(0x13);

        result->bios_boot_specification  = bf2.test(0);
        result->key_init_network_boot    = bf2.test(1);
        result->targeted_content_distrib = bf2.test(2);
        result->uefi                     = bf2.test(3);
        result->virtual_machine          = bf2.test(4);
    }
}

} // namespace aspia
