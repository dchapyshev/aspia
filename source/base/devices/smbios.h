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
    class TableEnumeratorImpl;
    struct Data;

public:
    static std::unique_ptr<SMBios> Create(std::unique_ptr<uint8_t[]> data, size_t data_size);

    uint8_t GetMajorVersion() const;
    uint8_t GetMinorVersion() const;

    template <class T>
    class TableEnumerator
    {
    public:
        TableEnumerator(const SMBios& smbios)
            : impl_(reinterpret_cast<const Data*>(smbios.data_.get()), T::TABLE_TYPE)
        {
            // Nothing
        }

        bool IsAtEnd() const { return impl_.IsAtEnd(); }
        void Advance() { impl_.Advance(T::TABLE_TYPE); }
        T GetTable() const { return T(TableReader(impl_.GetSMBiosData(), impl_.GetTableData())); }

    private:
        TableEnumeratorImpl impl_;
        DISALLOW_COPY_AND_ASSIGN(TableEnumerator);
    };

    class TableReader
    {
    public:
        TableReader(const TableReader& other);
        TableReader(const Data* smbios, const uint8_t* table);

        uint8_t GetMajorVersion() const { return smbios_->smbios_major_version; }
        uint8_t GetMinorVersion() const { return smbios_->smbios_minor_version; }
        uint8_t GetByte(uint8_t offset) const;
        uint16_t GetWord(uint8_t offset) const;
        uint32_t GetDword(uint8_t offset) const;
        uint64_t GetQword(uint8_t offset) const;
        std::string GetString(uint8_t offset) const;
        const uint8_t* GetPointer(uint8_t offset) const;
        uint8_t GetTableLength() const;

    private:
        const Data* smbios_;
        const uint8_t* table_;
    };

    class BiosTable
    {
    public:
        enum : uint8_t { TABLE_TYPE = 0x00 };

        using Feature = std::pair<std::string, bool>;
        using FeatureList = std::list<Feature>;

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
        friend class TableEnumerator<BiosTable>;
        BiosTable(const TableReader& reader);

        TableReader reader_;
    };

    class SystemTable
    {
    public:
        enum : uint8_t { TABLE_TYPE = 0x01 };

        std::string GetManufacturer() const;
        std::string GetProductName() const;
        std::string GetVersion() const;
        std::string GetSerialNumber() const;
        std::string GetUUID() const;
        std::string GetWakeupType() const;
        std::string GetSKUNumber() const;
        std::string GetFamily() const;

    private:
        friend class TableEnumerator<SystemTable>;
        SystemTable(const TableReader& reader);

        TableReader reader_;
    };

    class BaseboardTable
    {
    public:
        enum : uint8_t { TABLE_TYPE = 0x02 };

        using Feature = std::pair<std::string, bool>;
        using FeatureList = std::list<Feature>;

        enum class BoardType
        {
            UNKNOWN                      = 0,
            OTHER                        = 1,
            SERVER_BLADE                 = 2,
            CONNECTIVITY_SWITCH          = 3,
            SYSTEM_MANAGEMENT_MODULE     = 4,
            PROCESSOR_MODULE             = 5,
            IO_MODULE                    = 6,
            MEMORY_MODULE                = 7,
            DAUGHTER_BOARD               = 8,
            MOTHERBOARD                  = 9,
            PROCESSOR_PLUS_MEMORY_MODULE = 10,
            PROCESSOR_PLUS_IO_MODULE     = 11,
            INTERCONNECT_BOARD           = 12
        };

        std::string GetManufacturer() const;
        std::string GetProductName() const;
        std::string GetVersion() const;
        std::string GetSerialNumber() const;
        std::string GetAssetTag() const;
        FeatureList GetFeatures() const;
        std::string GetLocationInChassis() const;
        BoardType GetBoardType() const;

    private:
        friend class TableEnumerator<BaseboardTable>;
        BaseboardTable(const TableReader& reader);

        TableReader reader_;
    };

    class ChassisTable
    {
    public:
        enum : uint8_t { TABLE_TYPE = 0x03 };

        enum class Status
        {
            UNKNOWN         = 0,
            OTHER           = 1,
            SAFE            = 2,
            WARNING         = 3,
            CRITICAL        = 4,
            NON_RECOVERABLE = 5
        };

        enum class SecurityStatus
        {
            UNKNOWN                       = 0,
            OTHER                         = 1,
            NONE                          = 2,
            EXTERNAL_INTERFACE_LOCKED_OUT = 3,
            EXTERNAL_INTERFACE_ENABLED    = 4
        };

        enum class Type
        {
            UNKNOWN               = 0,
            OTHER                 = 1,
            DESKTOP               = 2,
            LOW_PROFILE_DESKTOP   = 3,
            PIZZA_BOX             = 4,
            MINI_TOWER            = 5,
            TOWER                 = 6,
            PORTABLE              = 7,
            LAPTOP                = 8,
            NOTEBOOK              = 9,
            HAND_HELD             = 10,
            DOCKING_STATION       = 11,
            ALL_IN_ONE            = 12,
            SUB_NOTEBOOK          = 13,
            SPACE_SAVING          = 14,
            LUNCH_BOX             = 15,
            MAIN_SERVER_CHASSIS   = 16,
            EXPANSION_CHASSIS     = 17,
            SUB_CHASSIS           = 18,
            BUS_EXPANSION_CHASSIS = 19,
            PERIPHERIAL_CHASSIS   = 20,
            RAID_CHASSIS          = 21,
            RACK_MOUNT_CHASSIS    = 22,
            SEALED_CASE_PC        = 23,
            MULTI_SYSTEM_CHASSIS  = 24,
            COMPACT_PCI           = 25,
            ADVANCED_TCA          = 26,
            BLADE                 = 27,
            BLADE_ENCLOSURE       = 28
        };

        std::string GetManufacturer() const;
        std::string GetVersion() const;
        std::string GetSerialNumber() const;
        std::string GetAssetTag() const;
        Type GetType() const;
        Status GetOSLoadStatus() const;
        Status GetPowerSourceStatus() const;
        Status GetTemperatureStatus() const;
        SecurityStatus GetSecurityStatus() const;
        int GetHeight() const; // In Units.
        int GetNumberOfPowerCords() const;

    private:
        friend class TableEnumerator<ChassisTable>;
        ChassisTable(const TableReader& reader);

        TableReader reader_;
    };

    class ProcessorTable
    {
    public:
        enum : uint8_t { TABLE_TYPE = 0x04 };

        enum class Type
        {
            OTHER             = 0x01,
            UNKNOWN           = 0x02,
            CENTRAL_PROCESSOR = 0x03,
            MATH_PROCESSOR    = 0x04,
            DSP_PROCESSOR     = 0x05,
            VIDEO_PROCESSOR   = 0x06
        };

        enum class Status
        {
            UNKNOWN          = 0x0,
            ENABLED          = 0x1,
            DISABLED_BY_USER = 0x2,
            DISABLED_BY_BIOS = 0x3,
            IDLE             = 0x4,
            OTHER            = 0x7
        };

        enum Characteristics
        {
            // Bit 0 Reserved.
            // Bit 1 Unknown.
            CHARACTERISTIC_64BIT_CAPABLE           = 0x0004,
            CHARACTERISTIC_MULTI_CORE              = 0x0008,
            CHARACTERISTIC_HARDWARE_THREAD         = 0x0010,
            CHARACTERISTIC_EXECUTE_PROTECTION      = 0x0020,
            CHARACTERISTIC_ENHANCED_VIRTUALIZATION = 0x0040,
            CHARACTERISTIC_POWER_CONTROL           = 0x0080
            // Bits 8:15 Reserved.
        };

        std::string GetManufacturer() const;
        std::string GetVersion() const;
        std::string GetFamily() const;
        Type GetType() const;
        Status GetStatus() const;
        std::string GetSocket() const;
        std::string GetUpgrade() const;
        int GetExternalClock() const;
        int GetCurrentSpeed() const;
        int GetMaximumSpeed() const;
        double GetVoltage() const;
        std::string GetSerialNumber() const;
        std::string GetAssetTag() const;
        std::string GetPartNumber() const;
        int GetCoreCount() const;
        int GetCoreEnabled() const;
        int GetThreadCount() const;
        uint16_t GetCharacteristics() const;

    private:
        friend class TableEnumerator<ProcessorTable>;
        ProcessorTable(const TableReader& reader);

        TableReader reader_;
    };

    class CacheTable
    {
    public:
        enum : uint8_t { TABLE_TYPE = 0x07 };

        enum class Location
        {
            UNKNOWN  = 0,
            INTERNAL = 1,
            EXTERNAL = 2,
            RESERVED = 3
        };

        enum class Status
        {
            UNKNOWN  = 0,
            ENABLED  = 1,
            DISABLED = 2
        };

        enum class Mode
        {
            UNKNOWN                   = 0,
            WRITE_THRU                = 1,
            WRITE_BACK                = 2,
            WRITE_WITH_MEMORY_ADDRESS = 3
        };

        enum class Level
        {
            UNKNOWN = 0,
            L1      = 1,
            L2      = 2,
            L3      = 3
        };

        enum SRAMType
        {
            SRAM_TYPE_OTHER          = 1,
            SRAM_TYPE_UNKNOWN        = 2,
            SRAM_TYPE_NON_BURST      = 4,
            SRAM_TYPE_BURST          = 8,
            SRAM_TYPE_PIPELINE_BURST = 16,
            SRAM_TYPE_SYNCHRONOUS    = 32,
            SRAM_TYPE_ASYNCHRONOUS   = 64
        };

        enum class ErrorCorrectionType
        {
            UNKNOWN        = 0,
            OTHER          = 1,
            NONE           = 2,
            PARITY         = 3,
            SINGLE_BIT_ECC = 4,
            MULTI_BIT_ECC  = 5
        };

        enum class Type
        {
            UNKNOWN     = 0,
            OTHER       = 1,
            INSTRUCTION = 2,
            DATA        = 3,
            UNIFIED     = 4
        };

        enum class Associativity
        {
            UNKNOWN                = 0,
            OTHER                  = 1,
            DIRECT_MAPPED          = 2,
            WAY_2_SET_ASSOCIATIVE  = 3,
            WAY_4_SET_ASSOCIATIVE  = 4,
            FULLY_ASSOCIATIVE      = 5,
            WAY_8_SET_ASSOCIATIVE  = 6,
            WAY_16_SET_ASSOCIATIVE = 7,
            WAY_12_SET_ASSOCIATIVE = 8,
            WAY_24_SET_ASSOCIATIVE = 9,
            WAY_32_SET_ASSOCIATIVE = 10,
            WAY_48_SET_ASSOCIATIVE = 11,
            WAY_64_SET_ASSOCIATIVE = 12,
            WAY_20_SET_ASSOCIATIVE = 13
        };

        std::string GetName() const;
        Location GetLocation() const;
        Status GetStatus() const;
        Mode GetMode() const;
        Level GetLevel() const;
        int GetMaximumSize() const;
        int GetCurrentSize() const;
        uint16_t GetSupportedSRAMTypes() const;
        SRAMType GetCurrentSRAMType() const;
        int GetSpeed() const;
        ErrorCorrectionType GetErrorCorrectionType() const;
        Type GetType() const;
        Associativity GetAssociativity() const;

    private:
        friend class TableEnumerator<CacheTable>;
        CacheTable(const TableReader& reader);

        TableReader reader_;
    };

    class PortConnectorTable
    {
    public:
        enum : uint8_t { TABLE_TYPE = 0x08 };

        std::string GetInternalDesignation() const;
        std::string GetExternalDesignation() const;
        std::string GetType() const;
        std::string GetInternalConnectorType() const;
        std::string GetExternalConnectorType() const;

    private:
        friend class TableEnumerator<PortConnectorTable>;
        PortConnectorTable(const TableReader& reader);

        static std::string ConnectorTypeToString(uint8_t type);

        TableReader reader_;
    };

    class SystemSlotTable
    {
    public:
        enum : uint8_t { TABLE_TYPE = 0x09 };

        enum class Usage
        {
            OTHER     = 0x01,
            UNKNOWN   = 0x02,
            AVAILABLE = 0x03,
            IN_USE    = 0x04
        };

        enum class Length
        {
            OTHER   = 0x01,
            UNKNOWN = 0x02,
            SHORT   = 0x03,
            LONG    = 0x04
        };

        std::string GetSlotDesignation() const;
        std::string GetType() const;
        Usage GetUsage() const;
        std::string GetBusWidth() const;
        Length GetLength() const;

    private:
        friend class TableEnumerator<SystemSlotTable>;
        SystemSlotTable(const TableReader& reader);

        TableReader reader_;
    };

    class OnBoardDeviceTable
    {
    public:
        enum : uint8_t { TABLE_TYPE = 0x0A };

        int GetDeviceCount() const;
        std::string GetDescription(int index) const;
        std::string GetType(int index) const;
        bool IsEnabled(int index) const;

    private:
        friend class TableEnumerator<OnBoardDeviceTable>;
        OnBoardDeviceTable(const TableReader& reader);

        const int count_;
        const uint8_t* ptr_;
        TableReader reader_;
    };

    class MemoryDeviceTable
    {
    public:
        enum : uint8_t { TABLE_TYPE = 0x11 };

        std::string GetDeviceLocator() const;
        int GetSize() const;
        std::string GetType() const;
        int GetSpeed() const;
        std::string GetFormFactor() const;
        std::string GetSerialNumber() const;
        std::string GetPartNumber() const;
        std::string GetManufacturer() const;
        std::string GetBank() const;
        int GetTotalWidth() const;
        int GetDataWidth() const;

    private:
        friend class TableEnumerator<MemoryDeviceTable>;
        MemoryDeviceTable(const TableReader& reader);

        TableReader reader_;
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

    class TableEnumeratorImpl
    {
    public:
        TableEnumeratorImpl(const Data* data, uint8_t type);

        bool IsAtEnd() const;
        void Advance(uint8_t type);
        const Data* GetSMBiosData() const;
        const uint8_t* GetTableData() const;

    private:
        const Data* data_;
        const uint8_t* start_;
        const uint8_t* next_;
        const uint8_t* end_;
        const uint8_t* current_;
        DISALLOW_COPY_AND_ASSIGN(TableEnumeratorImpl);
    };

    std::unique_ptr<uint8_t[]> data_;
    const size_t data_size_;

    DISALLOW_COPY_AND_ASSIGN(SMBios);
};

} // namespace aspia

#endif // _ASPIA_BASE__DEVICES__SMBIOS_H
