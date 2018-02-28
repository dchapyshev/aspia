//
// PROJECT:         Aspia
// FILE:            base/devices/smbios.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__DEVICES__SMBIOS_H
#define _ASPIA_BASE__DEVICES__SMBIOS_H

#include "base/macros.h"

#include <memory>
#include <string>

namespace aspia {

class SMBios
{
    struct Data;

public:
    class TableEnumerator;

    static std::unique_ptr<SMBios> Create(std::unique_ptr<uint8_t[]> data, size_t data_size);

    uint8_t GetMajorVersion() const;
    uint8_t GetMinorVersion() const;

    enum TableType : uint8_t
    {
        TABLE_TYPE_BIOS             = 0x00,
        TABLE_TYPE_SYSTEM           = 0x01,
        TABLE_TYPE_BASEBOARD        = 0x02,
        TABLE_TYPE_CHASSIS          = 0x03,
        TABLE_TYPE_PROCESSOR        = 0x04,
        TABLE_TYPE_CACHE            = 0x07,
        TABLE_TYPE_PORT_CONNECTOR   = 0x08,
        TABLE_TYPE_SYSTEM_SLOT      = 0x09,
        TABLE_TYPE_ONBOARD_DEVICE   = 0x0A,
        TABLE_TYPE_MEMORY_DEVICE    = 0x11,
        TABLE_TYPE_POINTING_DEVICE  = 0x15,
        TABLE_TYPE_PORTABLE_BATTERY = 0x16
    };

    class Table
    {
    public:
        Table(const Table& other);
        Table& operator=(const Table& other);

        uint8_t Byte(uint8_t offset) const;
        uint16_t Word(uint8_t offset) const;
        uint32_t Dword(uint8_t offset) const;
        uint64_t Qword(uint8_t offset) const;
        std::string String(uint8_t offset) const;
        const uint8_t* Pointer(uint8_t offset) const;
        uint8_t Length() const;

    private:
        friend class TableEnumerator;
        Table(const uint8_t* table);

        const uint8_t* table_;
    };

    class TableEnumerator
    {
    public:
        TableEnumerator(const SMBios& smbios, TableType table_type);

        bool IsAtEnd() const;
        void Advance();
        Table GetTable() const;

    private:
        const TableType table_type_;
        const Data* data_;
        const uint8_t* start_;
        const uint8_t* next_;
        const uint8_t* end_;
        const uint8_t* current_;

        DISALLOW_COPY_AND_ASSIGN(TableEnumerator);
    };

private:
    SMBios(std::unique_ptr<uint8_t[]> data, size_t data_size);
    static int GetTableCount(const uint8_t* table_data, uint32_t length);

    static const size_t kMaxDataSize = 0xFA00; // 64K

    struct Data
    {
        uint8_t used_20_calling_method;
        uint8_t smbios_major_version;
        uint8_t smbios_minor_version;
        uint8_t dmi_revision;
        uint32_t length;
        uint8_t smbios_table_data[kMaxDataSize];
    };

    std::unique_ptr<uint8_t[]> data_;
    const size_t data_size_;

    DISALLOW_COPY_AND_ASSIGN(SMBios);
};

} // namespace aspia

#endif // _ASPIA_BASE__DEVICES__SMBIOS_H
