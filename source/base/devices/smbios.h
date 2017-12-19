//
// PROJECT:         Aspia
// FILE:            base/devices/smbios.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__DEVICES__SMBIOS_H
#define _ASPIA_BASE__DEVICES__SMBIOS_H

#include "proto/system_info_session_message.pb.h"
#include "base/macros.h"

#include <memory>
#include <string>

namespace aspia {

class SMBios
{
    struct Data;

public:
    static std::unique_ptr<SMBios> Create(std::unique_ptr<uint8_t[]> data, size_t data_size);

    uint8_t GetMajorVersion() const;
    uint8_t GetMinorVersion() const;

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

    enum TableType : uint8_t
    {
        TABLE_TYPE_BIOS           = 0x00,
        TABLE_TYPE_SYSTEM         = 0x01,
        TABLE_TYPE_BASEBOARD      = 0x02,
        TABLE_TYPE_CHASSIS        = 0x03,
        TABLE_TYPE_PROCESSOR      = 0x04,
        TABLE_TYPE_CACHE          = 0x07,
        TABLE_TYPE_PORT_CONNECTOR = 0x08,
        TABLE_TYPE_SYSTEM_SLOT    = 0x09,
        TABLE_TYPE_MEMORY_DEVICE  = 0x11
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

    class TableEnumeratorNew
    {
    public:
        TableEnumeratorNew(const SMBios& smbios, TableType table_type)
            : impl_(reinterpret_cast<const Data*>(smbios.data_.get()), table_type),
              table_type_(table_type)
        {
            // Nothing
        }

        bool IsAtEnd() const { return impl_.IsAtEnd(); }
        void Advance() { impl_.Advance(table_type_); }
        TableReader GetTable() const { return TableReader(impl_.GetSMBiosData(), impl_.GetTableData()); }

    private:
        const TableType table_type_;
        TableEnumeratorImpl impl_;
        DISALLOW_COPY_AND_ASSIGN(TableEnumeratorNew);
    };

    class OnBoardDeviceTable
    {
    public:
        enum : uint8_t { TABLE_TYPE = 0x0A };

        int GetDeviceCount() const;
        std::string GetDescription(int index) const;
        proto::DmiOnBoardDevices::Type GetType(int index) const;
        bool IsEnabled(int index) const;

    private:
        friend class TableEnumerator<OnBoardDeviceTable>;
        explicit OnBoardDeviceTable(const TableReader& reader);

        const int count_;
        const uint8_t* ptr_;
        TableReader reader_;
    };

    class PointingDeviceTable
    {
    public:
        enum : uint8_t { TABLE_TYPE = 0x15 };

        proto::DmiPointingDevices::Type GetDeviceType() const;
        proto::DmiPointingDevices::Interface GetInterface() const;
        int GetButtonCount() const;

    private:
        friend class TableEnumerator<PointingDeviceTable>;
        explicit PointingDeviceTable(const TableReader& reader);

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
        proto::DmiPortableBattery::Chemistry GetChemistry() const;
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

    std::unique_ptr<uint8_t[]> data_;
    const size_t data_size_;

    DISALLOW_COPY_AND_ASSIGN(SMBios);
};

} // namespace aspia

#endif // _ASPIA_BASE__DEVICES__SMBIOS_H
