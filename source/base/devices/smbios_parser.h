//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/devices/smbios_parser.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__DEVICES__SMBIOS_PARSER_H
#define _ASPIA_BASE__DEVICES__SMBIOS_PARSER_H

#include "base/macros.h"

#include <memory>
#include <string>
#include <vector>

namespace aspia {

class SMBiosParser
{
public:
    static std::unique_ptr<SMBiosParser> Create(std::unique_ptr<uint8_t[]> data, size_t data_size);

    static const size_t kMaxDataSize = 0xFA00; // 64K

    struct SMBiosData
    {
        uint8_t used_20_calling_method;
        uint8_t smbios_major_version;
        uint8_t smbios_minor_version;
        uint8_t dmi_revision;
        uint32_t length;
        uint8_t smbios_table_data[kMaxDataSize];
    };

    enum TableType : uint8_t
    {
        TABLE_TYPE_BIOS                                 = 0,
        TABLE_TYPE_SYSTEM                               = 1,
        TABLE_TYPE_BASEBOARD                            = 2,
        TABLE_TYPE_CHASSIS                              = 3,
        TABLE_TYPE_PROCESSOR                            = 4,
        TABLE_TYPE_MEMORY_CONTROLLER                    = 5,
        TABLE_TYPE_MEMORY_MODULE                        = 6,
        TABLE_TYPE_CACHE                                = 7,
        TABLE_TYPE_PORT_CONNECTOR                       = 8,
        TABLE_TYPE_SYSTEM_SLOTS                         = 9,
        TABLE_TYPE_ONBOARD_DEVICES                      = 10,
        TABLE_TYPE_OEM_STRINGS                          = 11,
        TABLE_TYPE_SYSTEM_CONFIGURATION_OPTIONS         = 12,
        TABLE_TYPE_BIOS_LANGUAGE                        = 13,
        TABLE_TYPE_GROUP_ASSOCIATIONS                   = 14,
        TABLE_TYPE_SYSTEM_EVENT_LOG                     = 15,
        TABLE_TYPE_PHYSICAL_MEMORY_ARRAY                = 16,
        TABLE_TYPE_MEMORY_DEVICE                        = 17,
        TABLE_TYPE_32BIT_MEMORY_ERROR                   = 18,
        TABLE_TYPE_MEMORY_ARRAY_MAPPED_ADDRESS          = 19,
        TABLE_TYPE_MEMORY_DEVICE_MAPPED_ADDRESS         = 20,
        TABLE_TYPE_BUILDIN_POINTING_DEVICE              = 21,
        TABLE_TYPE_PORTABLE_BATTERY                     = 22,
        TABLE_TYPE_SYSTEM_RESET                         = 23,
        TABLE_TYPE_HARDWARE_SECURITY                    = 24,
        TABLE_TYPE_SYSTEM_POWER_CONTROLS                = 25,
        TABLE_TYPE_VOLTAGE_PROBE                        = 26,
        TABLE_TYPE_COOLING_DEVICE                       = 27,
        TABLE_TYPE_TEMPERATURE_PROBE                    = 28,
        TABLE_TYPE_ELECTRICAL_CURRENT_PROBE             = 29,
        TABLE_TYPE_OUT_OF_BAND_REMOTE_ACCESS            = 30,
        TABLE_TYPE_BIS_ENTRY_POINT                      = 31,
        TABLE_TYPE_SYSTEM_BOOT                          = 32,
        TABLE_TYPE_64BIT_MEMORY_ERROR                   = 33,
        TABLE_TYPE_MANAGEMENT_DEVICE                    = 34,
        TABLE_TYPE_MANAGEMENT_DEVICE_COMPONENT          = 35,
        TABLE_TYPE_MANAGEMENT_DEVICE_THRESHOLD_DATA     = 36,
        TABLE_TYPE_MEMORY_CHANNEL                       = 37,
        TABLE_TYPE_IPMI_DEVICE                          = 38,
        TABLE_TYPE_SYSTEM_POWER_SUPPLY                  = 39,
        TABLE_TYPE_ADDITIONAL                           = 40,
        TABLE_TYPE_ONBOARD_DEVICE_EXTENDED              = 41,
        TABLE_TYPE_MANAGEMENT_CONTROLLER_HOST_INTERFACE = 42,
        TABLE_TYPE_INACTIVE                             = 126,
        TABLE_TYPE_END_OF_TABLE                         = 127
    };

    struct Table
    {
        uint8_t type;
        uint8_t length;
        uint16_t handle;
        // data
    };

    Table* GetTable(TableType type);

    class TableParser
    {
    public:
        virtual ~TableParser() = default;

    protected:
        TableParser(const Table* table);

        uint8_t GetByte(uint8_t offset) const;
        uint16_t GetWord(uint8_t offset) const;
        uint32_t GetDword(uint8_t offset) const;
        uint64_t GetQword(uint8_t offset) const;
        std::string GetString(uint8_t offset) const;

    private:
        const uint8_t* data_;
    };

    class BiosTableParser : private TableParser
    {
    public:
        BiosTableParser(Table* table);

        std::string GetManufacturer() const;
        std::string GetVersion() const;
        std::string GetDate() const;
        int GetSize() const; // In kB.
        std::string GetBiosRevision() const;
        std::string GetFirmwareRevision() const;
        std::string GetAddress() const;
        int GetRuntimeSize() const; // In bytes.

        using Characteristic = std::pair<std::string, bool>;
        std::vector<Characteristic> GetCharacteristics() const;

    private:
        DISALLOW_COPY_AND_ASSIGN(BiosTableParser);
    };

private:
    SMBiosParser(std::unique_ptr<uint8_t[]> data, size_t data_size, int table_count);

    static int GetTableCount(uint8_t* table_data, uint32_t length);
    static std::string GetString(const Table* table, uint8_t offset);

    std::unique_ptr<uint8_t[]> data_;
    const size_t data_size_;
    const int table_count_;

    DISALLOW_COPY_AND_ASSIGN(SMBiosParser);
};

} // namespace aspia

#endif // _ASPIA_BASE__DEVICES__SMBIOS_PARSER_H
