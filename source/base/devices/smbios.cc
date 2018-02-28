//
// PROJECT:         Aspia
// FILE:            base/devices/smbios.cc
// LICENSE:         GNU Lesser General Public License 2.1
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
        DLOG(LS_WARNING) << "SMBios tables not found";
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
// TableEnumerator
//

SMBios::TableEnumerator::TableEnumerator(const SMBios& smbios, TableType table_type)
    : data_(reinterpret_cast<const Data*>(smbios.data_.get())),
      table_type_(table_type)
{
    start_ = &data_->smbios_table_data[0];
    end_ = start_ + data_->length;
    current_ = start_;
    next_ = start_;

    Advance();
}

bool SMBios::TableEnumerator::IsAtEnd() const
{
    return current_ == nullptr;
}

void SMBios::TableEnumerator::Advance()
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
            DLOG(LS_WARNING) << "Invalid SMBIOS table length: " << table_length;
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
        if (table_type == table_type_)
            return;

        current_ = next_;
    }

    current_ = nullptr;
    next_ = nullptr;
}

SMBios::Table SMBios::TableEnumerator::GetTable() const
{
    return Table(current_);
}

//
// SMBiosTable
//

SMBios::Table::Table(const Table& other)
    : table_(other.table_)
{
    // Nothing
}

SMBios::Table::Table(const uint8_t* table)
    : table_(table)
{
    DCHECK(table_);
}

SMBios::Table& SMBios::Table::operator=(const Table& other)
{
    table_ = other.table_;
    return *this;
}

uint8_t SMBios::Table::Byte(uint8_t offset) const
{
    return table_[offset];
}

uint16_t SMBios::Table::Word(uint8_t offset) const
{
    return *reinterpret_cast<const uint16_t*>(Pointer(offset));
}

uint32_t SMBios::Table::Dword(uint8_t offset) const
{
    return *reinterpret_cast<const uint32_t*>(Pointer(offset));
}

uint64_t SMBios::Table::Qword(uint8_t offset) const
{
    return *reinterpret_cast<const uint64_t*>(Pointer(offset));
}

std::string SMBios::Table::String(uint8_t offset) const
{
    uint8_t handle = Byte(offset);
    if (!handle)
        return std::string();

    char* string = reinterpret_cast<char*>(
        const_cast<uint8_t*>(Pointer(0))) + Length();

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

const uint8_t* SMBios::Table::Pointer(uint8_t offset) const
{
    return &table_[offset];
}

uint8_t SMBios::Table::Length() const
{
    return Byte(0x01);
}

} // namespace aspia
