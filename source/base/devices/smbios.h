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

        TableReader& operator=(const TableReader& other);

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
        explicit BiosTable(const TableReader& reader);

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
        explicit SystemTable(const TableReader& reader);

        TableReader reader_;
    };

    class BaseboardTable
    {
    public:
        enum : uint8_t { TABLE_TYPE = 0x02 };

        using Feature = std::pair<std::string, bool>;
        using FeatureList = std::list<Feature>;

        std::string GetManufacturer() const;
        std::string GetProductName() const;
        std::string GetVersion() const;
        std::string GetSerialNumber() const;
        std::string GetAssetTag() const;
        FeatureList GetFeatures() const;
        std::string GetLocationInChassis() const;
        std::string GetBoardType() const;

    private:
        friend class TableEnumerator<BaseboardTable>;
        explicit BaseboardTable(const TableReader& reader);

        TableReader reader_;
    };

    class ChassisTable
    {
    public:
        enum : uint8_t { TABLE_TYPE = 0x03 };

        std::string GetManufacturer() const;
        std::string GetVersion() const;
        std::string GetSerialNumber() const;
        std::string GetAssetTag() const;
        std::string GetType() const;
        std::string GetOSLoadStatus() const;
        std::string GetPowerSourceStatus() const;
        std::string GetTemperatureStatus() const;
        std::string GetSecurityStatus() const;
        int GetHeight() const; // In Units.
        int GetNumberOfPowerCords() const;

    private:
        friend class TableEnumerator<ChassisTable>;
        explicit ChassisTable(const TableReader& reader);

        static std::string StatusToString(uint8_t status);

        TableReader reader_;
    };

    class ProcessorTable
    {
    public:
        enum : uint8_t { TABLE_TYPE = 0x04 };

        using Feature = std::pair<std::string, bool>;
        using FeatureList = std::list<Feature>;

        std::string GetManufacturer() const;
        std::string GetVersion() const;
        std::string GetFamily() const;
        std::string GetType() const;
        std::string GetStatus() const;
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
        FeatureList GetFeatures() const;

    private:
        friend class TableEnumerator<ProcessorTable>;
        explicit ProcessorTable(const TableReader& reader);

        TableReader reader_;
    };

    class CacheTable
    {
    public:
        enum : uint8_t { TABLE_TYPE = 0x07 };

        using SRAMType = std::pair<std::string, bool>;
        using SRAMTypeList = std::list<SRAMType>;

        std::string GetName() const;
        std::string GetLocation() const;
        bool IsEnabled() const;
        std::string GetMode() const;
        int GetLevel() const;
        int GetMaximumSize() const;
        int GetCurrentSize() const;
        SRAMTypeList GetSupportedSRAMTypes() const;
        std::string GetCurrentSRAMType() const;
        int GetSpeed() const;
        std::string GetErrorCorrectionType() const;
        std::string GetType() const;
        std::string GetAssociativity() const;

    private:
        friend class TableEnumerator<CacheTable>;
        explicit CacheTable(const TableReader& reader);

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
        explicit PortConnectorTable(const TableReader& reader);

        static std::string ConnectorTypeToString(uint8_t type);

        TableReader reader_;
    };

    class SystemSlotTable
    {
    public:
        enum : uint8_t { TABLE_TYPE = 0x09 };

        std::string GetSlotDesignation() const;
        std::string GetType() const;
        std::string GetUsage() const;
        std::string GetBusWidth() const;
        std::string GetLength() const;

    private:
        friend class TableEnumerator<SystemSlotTable>;
        explicit SystemSlotTable(const TableReader& reader);

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
        explicit OnBoardDeviceTable(const TableReader& reader);

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
        explicit MemoryDeviceTable(const TableReader& reader);

        TableReader reader_;
    };

    class BuildinPointingTable
    {
    public:
        enum : uint8_t { TABLE_TYPE = 0x15 };

        std::string GetDeviceType() const;
        std::string GetInterface() const;
        int GetButtonCount() const;

    private:
        friend class TableEnumerator<BuildinPointingTable>;
        explicit BuildinPointingTable(const TableReader& reader);

        TableReader reader_;
    };

    class PortableBatteryTable
    {
    public:
        enum : uint8_t { TABLE_TYPE = 0x16 };

        std::string GetLocation() const;
        std::string GetManufacturer() const;
        std::string GetManufactureDate() const;
        std::string GetSerialNumber() const;
        std::string GetDeviceName() const;
        std::string GetChemistry() const;
        int GetDesignCapacity() const;
        int GetDesignVoltage() const;
        std::string GetSBDSVersionNumber() const;
        int GetMaxErrorInBatteryData() const;
        std::string GetSBDSSerialNumber() const;
        std::string GetSBDSManufactureDate() const;
        std::string GetSBDSDeviceChemistry() const;

    private:
        friend class TableEnumerator<PortableBatteryTable>;
        explicit PortableBatteryTable(const TableReader& reader);

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
