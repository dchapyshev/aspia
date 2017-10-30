//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/devices/smbios.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__DEVICES__SMBIOS_H
#define _ASPIA_BASE__DEVICES__SMBIOS_H

#include "base/macros.h"

#include <memory>
#include <string>
#include <list>

namespace aspia {

class SMBios
{
public:
    static std::unique_ptr<SMBios> Create(std::unique_ptr<uint8_t[]> data, size_t data_size);

    uint8_t GetMajorVersion() const;
    uint8_t GetMinorVersion() const;

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

    class Table
    {
    public:
        virtual ~Table() = default;

        using Feature = std::pair<std::string, bool>;
        using FeatureList = std::list<Feature>;

        bool IsValid() const;

    protected:
        Table(const SMBios& smbios, TableType type);

        uint8_t GetByte(uint8_t offset) const;
        uint16_t GetWord(uint8_t offset) const;
        uint32_t GetDword(uint8_t offset) const;
        uint64_t GetQword(uint8_t offset) const;
        std::string GetString(uint8_t offset) const;
        const uint8_t* GetPointer(uint8_t offset) const;

    private:
        const uint8_t* data_;
    };

    class BiosTable : public Table
    {
    public:
        BiosTable(const SMBios& smbios);

        std::string GetManufacturer() const;
        std::string GetVersion() const;
        std::string GetDate() const;
        int GetSize() const; // In kB.
        std::string GetBiosRevision() const;
        std::string GetFirmwareRevision() const;
        std::string GetAddress() const;
        int GetRuntimeSize() const; // In bytes.
        FeatureList GetCharacteristics() const;

    private:
        DISALLOW_COPY_AND_ASSIGN(BiosTable);
    };

    class SystemTable : public Table
    {
    public:
        SystemTable(const SMBios& smbios);

        std::string GetManufacturer() const;
        std::string GetProductName() const;
        std::string GetVersion() const;
        std::string GetSerialNumber() const;
        std::string GetUUID() const;
        std::string GetWakeupType() const;
        std::string GetSKUNumber() const;
        std::string GetFamily() const;

    private:
        const uint8_t major_version_;
        const uint8_t minor_version_;
        DISALLOW_COPY_AND_ASSIGN(SystemTable);
    };

    class BaseboardTable : public Table
    {
    public:
        BaseboardTable(const SMBios& smbios);

        std::string GetManufacturer() const;
        std::string GetProductName() const;
        std::string GetVersion() const;
        std::string GetSerialNumber() const;
        std::string GetAssetTag() const;
        FeatureList GetFeatures() const;
        std::string GetLocationInChassis() const;
        std::string GetBoardType() const;

    private:
        DISALLOW_COPY_AND_ASSIGN(BaseboardTable);
    };

private:
    friend class SMBiosTable;

    SMBios(std::unique_ptr<uint8_t[]> data, size_t data_size, int table_count);

    static int GetTableCount(const uint8_t* table_data, uint32_t length);
    const uint8_t* GetTable(TableType type) const;

    std::unique_ptr<uint8_t[]> data_;
    const size_t data_size_;
    const int table_count_;

    DISALLOW_COPY_AND_ASSIGN(SMBios);
};

} // namespace aspia

#endif // _ASPIA_BASE__DEVICES__SMBIOS_H
