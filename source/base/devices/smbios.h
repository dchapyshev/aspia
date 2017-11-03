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
        T GetTable() const { return T(impl_.GetSMBiosData(), impl_.GetTableData()); }

    private:
        TableEnumeratorImpl impl_;
        DISALLOW_COPY_AND_ASSIGN(TableEnumerator);
    };

    class TableReader
    {
    public:
        TableReader(const Data* smbios, const uint8_t* table);

        uint8_t GetMajorVersion() const { return smbios_->smbios_major_version; }
        uint8_t GetMinorVersion() const { return smbios_->smbios_minor_version; }
        uint8_t GetByte(uint8_t offset) const;
        uint16_t GetWord(uint8_t offset) const;
        uint32_t GetDword(uint8_t offset) const;
        uint64_t GetQword(uint8_t offset) const;
        std::string GetString(uint8_t offset) const;
        const uint8_t* GetPointer(uint8_t offset) const;

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
        BiosTable(const Data* smbios, const uint8_t* table);

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
        SystemTable(const Data* smbios, const uint8_t* table);

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
        BaseboardTable(const Data* smbios, const uint8_t* table);

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
