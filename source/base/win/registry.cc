//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/win/registry.h"
#include "base/logging.h"

namespace base {

namespace {

// RegEnumValue() reports the number of characters from the name that were
// written to the buffer, not how many there are. This constant is the maximum
// name size, such that a buffer with this size should read any name.
constexpr DWORD MAX_REGISTRY_NAME_SIZE = 16384;

//--------------------------------------------------------------------------------------------------
// Registry values are read as BYTE* but can have wchar_t* data whose last
// wchar_t is truncated. This function converts the reported |byte_size| to
// a size in wchar_t that can store a truncated wchar_t if necessary.
DWORD to_wchar_size(DWORD byte_size)
{
    return (byte_size + sizeof(wchar_t) - 1) / sizeof(wchar_t);
}

// Mask to pull WOW64 access flags out of REGSAM access.
constexpr REGSAM kWow64AccessMask = KEY_WOW64_32KEY | KEY_WOW64_64KEY;

//--------------------------------------------------------------------------------------------------
wchar_t* writeInto(std::wstring* str, size_t length_with_null)
{
    DCHECK(length_with_null > 1u);
    str->reserve(length_with_null);
    str->resize(length_with_null - 1);
    return &((*str)[0]);
}

} // namespace

//--------------------------------------------------------------------------------------------------
RegistryKey::RegistryKey(HKEY key)
    : key_(key)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
RegistryKey::RegistryKey(HKEY rootkey, const QString& subkey, REGSAM access)
{
    if (rootkey)
    {
        if (access & (KEY_SET_VALUE | KEY_CREATE_SUB_KEY | KEY_CREATE_LINK))
            create(rootkey, subkey, access);
        else
            open(rootkey, subkey, access);
    }
    else
    {
        wow64access_ = access & kWow64AccessMask;
    }
}

//--------------------------------------------------------------------------------------------------
RegistryKey::RegistryKey(RegistryKey&& other) noexcept
{
    key_ = other.key_;
    wow64access_ = other.wow64access_;

    other.key_ = nullptr;
    other.wow64access_ = 0;
}

//--------------------------------------------------------------------------------------------------
RegistryKey& RegistryKey::operator=(RegistryKey&& other) noexcept
{
    close();

    key_ = other.key_;
    wow64access_ = other.wow64access_;

    other.key_ = nullptr;
    other.wow64access_ = 0;

    return *this;
}

//--------------------------------------------------------------------------------------------------
RegistryKey::~RegistryKey()
{
    close();
}

//--------------------------------------------------------------------------------------------------
bool RegistryKey::isValid() const
{
    return (key_ != nullptr);
}

//--------------------------------------------------------------------------------------------------
LONG RegistryKey::create(HKEY rootkey, const QString& subkey, REGSAM access)
{
    DWORD disposition_value;

    return createWithDisposition(rootkey, subkey, &disposition_value, access);
}

//--------------------------------------------------------------------------------------------------
LONG RegistryKey::createWithDisposition(HKEY rootkey, const QString& subkey,
                                        DWORD* disposition, REGSAM access)
{
    DCHECK(rootkey && access && disposition);

    HKEY subhkey = nullptr;

    LONG result = RegCreateKeyExW(rootkey,
                                  qUtf16Printable(subkey),
                                  0,
                                  nullptr,
                                  REG_OPTION_NON_VOLATILE,
                                  access,
                                  nullptr,
                                  &subhkey,
                                  disposition);
    if (result == ERROR_SUCCESS)
    {
        close();
        key_ = subhkey;
        wow64access_ = access & kWow64AccessMask;
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
LONG RegistryKey::createKey(const QString& name, REGSAM access)
{
    DCHECK(access);

    // After the application has accessed an alternate registry view using one of the
    // [KEY_WOW64_32KEY / KEY_WOW64_64KEY] flags, all subsequent operations (create, delete, or
    // open) on child registry keys must explicitly use the same flag. Otherwise, there can be
    // unexpected behavior.
    // http://msdn.microsoft.com/en-us/library/windows/desktop/aa384129.aspx.
    if ((access & kWow64AccessMask) != wow64access_)
    {
        NOTREACHED();
        return ERROR_INVALID_PARAMETER;
    }
    HKEY subkey = nullptr;
    LONG result = RegCreateKeyExW(key_, qUtf16Printable(name), 0, nullptr,
                                  REG_OPTION_NON_VOLATILE, access, nullptr, &subkey, nullptr);
    if (result == ERROR_SUCCESS)
    {
        close();
        key_ = subkey;
        wow64access_ = access & kWow64AccessMask;
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
LONG RegistryKey::open(HKEY rootkey, const QString& subkey, REGSAM access)
{
    DCHECK(rootkey && access);

    HKEY subhkey = nullptr;

    LONG result = RegOpenKeyExW(rootkey, qUtf16Printable(subkey), 0, access, &subhkey);
    if (result == ERROR_SUCCESS)
    {
        close();
        key_ = subhkey;
        wow64access_ = access & kWow64AccessMask;
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
LONG RegistryKey::openKey(const QString& relative_key_name, REGSAM access)
{
    DCHECK(access);

    // After the application has accessed an alternate registry view using one of the
    // [KEY_WOW64_32KEY / KEY_WOW64_64KEY] flags, all subsequent operations (create, delete,
    // or open) on child registry keys must explicitly use the same flag. Otherwise, there can
    // be unexpected behavior.
    // http://msdn.microsoft.com/en-us/library/windows/desktop/aa384129.aspx.
    if ((access & kWow64AccessMask) != wow64access_)
    {
        NOTREACHED();
        return ERROR_INVALID_PARAMETER;
    }

    HKEY subkey = nullptr;
    LONG result = RegOpenKeyExW(key_, qUtf16Printable(relative_key_name), 0, access, &subkey);

    // We have to close the current opened key before replacing it with the new
    // one.
    if (result == ERROR_SUCCESS)
    {
        close();
        key_ = subkey;
        wow64access_ = access & kWow64AccessMask;
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
void RegistryKey::close()
{
    if (key_)
    {
        RegCloseKey(key_);
        key_ = nullptr;
        wow64access_ = 0;
    }
}

//--------------------------------------------------------------------------------------------------
bool RegistryKey::hasValue(const QString& name) const
{
    return (RegQueryValueExW(key_, qUtf16Printable(name), nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS);
}

//--------------------------------------------------------------------------------------------------
DWORD RegistryKey::valueCount() const
{
    DWORD count = 0;
    LONG result = RegQueryInfoKeyW(key_, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, &count,
                                   nullptr, nullptr, nullptr, nullptr);
    return (result == ERROR_SUCCESS) ? count : 0;
}

//--------------------------------------------------------------------------------------------------
LONG RegistryKey::readValue(const QString& name, void* data, DWORD* dsize, DWORD* dtype) const
{
    return RegQueryValueExW(key_, qUtf16Printable(name), nullptr, dtype, reinterpret_cast<LPBYTE>(data), dsize);
}

//--------------------------------------------------------------------------------------------------
LONG RegistryKey::readValueDW(const QString& name, DWORD* out_value) const
{
    DCHECK(out_value);

    DWORD type = REG_DWORD;
    DWORD size = sizeof(DWORD);
    DWORD local_value = 0;

    LONG result = readValue(name, &local_value, &size, &type);
    if (result == ERROR_SUCCESS)
    {
        if ((type == REG_DWORD || type == REG_BINARY) && size == sizeof(DWORD))
            *out_value = local_value;
        else
            result = ERROR_CANTREAD;
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
LONG RegistryKey::readInt64(const QString& name, qint64* out_value) const
{
    DCHECK(out_value);

    DWORD type = REG_QWORD;
    qint64 local_value = 0;
    DWORD size = sizeof(local_value);

    LONG result = readValue(name, &local_value, &size, &type);
    if (result == ERROR_SUCCESS)
    {
        if ((type == REG_QWORD || type == REG_BINARY) &&
            size == sizeof(local_value))
            *out_value = local_value;
        else
            result = ERROR_CANTREAD;
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
LONG RegistryKey::readValueBIN(const QString& name, QByteArray* out_value) const
{
    DCHECK(out_value);

    DWORD type = REG_BINARY;
    DWORD size = 0;

    LONG result = readValue(name, nullptr, &size, &type);
    if (result == ERROR_SUCCESS)
    {
        if (type != REG_BINARY || !size)
            return ERROR_CANTREAD;

        out_value->resize(size);

        result = readValue(name, out_value->data(), &size, &type);
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
LONG RegistryKey::readValue(const QString& name, QString* out_value) const
{
    DCHECK(out_value);

    const size_t kMaxStringLength = 1024;  // This is after expansion.
    // Use the one of the other forms of ReadValue if 1024 is too small for you.
    wchar_t raw_value[kMaxStringLength] = { 0 };
    DWORD type = REG_SZ;
    DWORD size = sizeof(raw_value);

    LONG result = readValue(name, raw_value, &size, &type);
    if (result == ERROR_SUCCESS)
    {
        if (type == REG_SZ)
        {
            *out_value = QString::fromWCharArray(raw_value);
        }
        else if (type == REG_EXPAND_SZ)
        {
            wchar_t expanded[kMaxStringLength];

            size = ExpandEnvironmentStringsW(raw_value, expanded, kMaxStringLength);

            //
            // Success: returns the number of wchar_t's copied
            // Fail: buffer too small, returns the size required
            // Fail: other, returns 0
            //
            if (size == 0 || size > kMaxStringLength)
            {
                result = ERROR_MORE_DATA;
            }
            else
            {
                *out_value = QString::fromWCharArray(expanded);
            }
        }
        else
        {
            // Not a string. Oops.
            result = ERROR_CANTREAD;
        }
    }

    return result;
}

LONG RegistryKey::readValue(const QString& name, std::wstring* out_value) const
{
    DCHECK(out_value);

    const size_t kMaxStringLength = 1024;  // This is after expansion.
    // Use the one of the other forms of ReadValue if 1024 is too small for you.
    wchar_t raw_value[kMaxStringLength] = { 0 };
    DWORD type = REG_SZ;
    DWORD size = sizeof(raw_value);

    LONG result = readValue(name, raw_value, &size, &type);
    if (result == ERROR_SUCCESS)
    {
        if (type == REG_SZ)
        {
            *out_value = raw_value;
        }
        else if (type == REG_EXPAND_SZ)
        {
            wchar_t expanded[kMaxStringLength];

            size = ExpandEnvironmentStringsW(raw_value, expanded, kMaxStringLength);

            //
            // Success: returns the number of wchar_t's copied
            // Fail: buffer too small, returns the size required
            // Fail: other, returns 0
            //
            if (size == 0 || size > kMaxStringLength)
            {
                result = ERROR_MORE_DATA;
            }
            else
            {
                *out_value = expanded;
            }
        }
        else
        {
            // Not a string. Oops.
            result = ERROR_CANTREAD;
        }
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
LONG RegistryKey::writeValue(const QString& name, const void* data, DWORD dsize, DWORD dtype)
{
    DCHECK(data || !dsize);

    return RegSetValueExW(key_, qUtf16Printable(name), 0, dtype,
                          reinterpret_cast<LPBYTE>(const_cast<void*>(data)), dsize);
}

//--------------------------------------------------------------------------------------------------
LONG RegistryKey::writeValue(const QString& name, DWORD in_value)
{
    return writeValue(name, &in_value, static_cast<DWORD>(sizeof(in_value)), REG_DWORD);
}

//--------------------------------------------------------------------------------------------------
LONG RegistryKey::writeValue(const QString& name, const QString& in_value)
{
    return writeValue(name, qUtf16Printable(in_value),
                      static_cast<DWORD>((in_value.length() + 1) * sizeof(wchar_t)), REG_SZ);
}

//--------------------------------------------------------------------------------------------------
LONG RegistryKey::deleteKey(const QString& name)
{
    DCHECK(key_);

    HKEY subkey = nullptr;

    // Verify the key exists before attempting delete to replicate previous behavior.
    LONG result = RegOpenKeyExW(key_, qUtf16Printable(name), 0, READ_CONTROL | wow64access_, &subkey);
    if (result != ERROR_SUCCESS)
        return result;

    RegCloseKey(subkey);

    return deleteKeyRecurse(key_, name, wow64access_);
}

//--------------------------------------------------------------------------------------------------
LONG RegistryKey::deleteEmptyKey(const QString& name)
{
    DCHECK(key_);

    HKEY target_key = nullptr;
    LONG result = RegOpenKeyExW(key_, qUtf16Printable(name), 0, KEY_READ | wow64access_, &target_key);
    if (result != ERROR_SUCCESS)
        return result;

    DWORD count = 0;
    result = RegQueryInfoKeyW(target_key, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, &count,
                              nullptr, nullptr, nullptr, nullptr);

    RegCloseKey(target_key);

    if (result != ERROR_SUCCESS)
        return result;

    if (count == 0)
        return RegDeleteKeyExW(key_, qUtf16Printable(name), wow64access_, 0);

    return ERROR_DIR_NOT_EMPTY;
}

//--------------------------------------------------------------------------------------------------
LONG RegistryKey::deleteValue(const QString& value_name)
{
    DCHECK(key_);
    LONG result = RegDeleteValueW(key_, qUtf16Printable(value_name));
    return result;
}

//--------------------------------------------------------------------------------------------------
// static
LONG RegistryKey::deleteKeyRecurse(HKEY root_key, const QString& name, REGSAM access)
{
    // First, see if the key can be deleted without having to recurse.
    LONG result = RegDeleteKeyExW(root_key, qUtf16Printable(name), access, 0);
    if (result == ERROR_SUCCESS)
        return result;

    HKEY target_key = nullptr;
    result = RegOpenKeyExW(root_key, qUtf16Printable(name), 0,
                           KEY_ENUMERATE_SUB_KEYS | access, &target_key);

    if (result == ERROR_FILE_NOT_FOUND)
        return ERROR_SUCCESS;
    if (result != ERROR_SUCCESS)
        return result;

    QString subkey_name = name;

    // Check for an ending slash and add one if it is missing.
    if (!name.isEmpty() && subkey_name[name.length() - 1] != '\\')
        subkey_name += "\\";

    // Enumerate the keys
    const DWORD kMaxKeyNameLength = MAX_PATH;
    const size_t base_key_length = subkey_name.length();
    std::wstring key_name;

    while (result == ERROR_SUCCESS)
    {
        DWORD key_size = kMaxKeyNameLength;
        result = RegEnumKeyExW(target_key,
                               0,
                               writeInto(&key_name, kMaxKeyNameLength),
                               &key_size,
                               nullptr,
                               nullptr,
                               nullptr,
                               nullptr);

        if (result != ERROR_SUCCESS)
            break;

        key_name.resize(key_size);
        subkey_name.resize(static_cast<QString::size_type>(base_key_length));
        subkey_name += QString::fromStdWString(key_name);

        if (deleteKeyRecurse(root_key, subkey_name, access) != ERROR_SUCCESS)
            break;
    }

    RegCloseKey(target_key);

    // Try again to delete the key.
    result = RegDeleteKeyExW(root_key, qUtf16Printable(name), access, 0);

    return result;
}

// RegistryValueIterator ---------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
RegistryValueIterator::RegistryValueIterator(HKEY root_key, const QString& folder_key,
                                             REGSAM wow64access) :
    name_(MAX_PATH, L'\0'),
    value_(MAX_PATH, L'\0')
{
    initialize(root_key, folder_key, wow64access);
}

//--------------------------------------------------------------------------------------------------
RegistryValueIterator::RegistryValueIterator(HKEY root_key, const QString& folder_key) :
    name_(MAX_PATH, '\0'),
    value_(MAX_PATH, L'\0')
{
    initialize(root_key, folder_key, 0);
}

//--------------------------------------------------------------------------------------------------
void RegistryValueIterator::initialize(HKEY root_key, const QString& folder_key, REGSAM wow64access)
{
    DCHECK_EQ((wow64access & ~kWow64AccessMask), static_cast<REGSAM>(0));

    LONG result = RegOpenKeyExW(root_key, qUtf16Printable(folder_key), 0,
                                KEY_READ | wow64access, &key_);
    if (result != ERROR_SUCCESS)
    {
        key_ = nullptr;
    }
    else
    {
        DWORD count = 0;
        result = RegQueryInfoKeyW(key_, nullptr, nullptr, nullptr, nullptr,
                                  nullptr, nullptr, &count, nullptr, nullptr,
                                  nullptr, nullptr);

        if (result != ERROR_SUCCESS)
        {
            RegCloseKey(key_);
            key_ = nullptr;
        }
        else
        {
            index_ = static_cast<int>(count - 1);
        }
    }

    read();
}

//--------------------------------------------------------------------------------------------------
RegistryValueIterator::~RegistryValueIterator()
{
    if (key_)
        RegCloseKey(key_);
}

//--------------------------------------------------------------------------------------------------
DWORD RegistryValueIterator::valueCount() const
{
    DWORD count = 0;
    LONG result = RegQueryInfoKeyW(key_, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                                   &count, nullptr, nullptr, nullptr, nullptr);
    if (result != ERROR_SUCCESS)
        return 0;

    return count;
}

//--------------------------------------------------------------------------------------------------
bool RegistryValueIterator::valid() const
{
    return key_ != nullptr && index_ >= 0;
}

//--------------------------------------------------------------------------------------------------
void RegistryValueIterator::operator++()
{
    --index_;
    read();
}

//--------------------------------------------------------------------------------------------------
bool RegistryValueIterator::read()
{
    if (valid())
    {
        DWORD capacity = static_cast<DWORD>(name_.capacity());
        DWORD name_size = capacity;

        // |value_size_| is in bytes. Reserve the last character for a NUL.
        value_size_ = static_cast<DWORD>((value_.size() - 1) * sizeof(wchar_t));

        LONG result = RegEnumValueW(key_, static_cast<DWORD>(index_), writeInto(&name_, name_size),
                                    &name_size, nullptr, &type_, reinterpret_cast<BYTE*>(value_.data()),
                                    &value_size_);
        if (result == ERROR_MORE_DATA)
        {
            // Registry key names are limited to 255 characters and fit within
            // MAX_PATH (which is 260) but registry value names can use up to 16,383
            // characters and the value itself is not limited
            // (from http://msdn.microsoft.com/en-us/library/windows/desktop/
            // ms724872(v=vs.85).aspx).
            // Resize the buffers and retry if their size caused the failure.
            DWORD value_size_in_wchars = to_wchar_size(value_size_);

            if (value_size_in_wchars + 1 > value_.size())
                value_.resize(value_size_in_wchars + 1, L'\0');

            value_size_ = static_cast<DWORD>((value_.size() - 1) * sizeof(wchar_t));
            name_size = name_size == capacity ? MAX_REGISTRY_NAME_SIZE : capacity;

            result = RegEnumValueW(key_, static_cast<DWORD>(index_), writeInto(&name_, name_size),
                                   &name_size, nullptr, &type_,
                                   reinterpret_cast<BYTE*>(value_.data()),
                                   &value_size_);
        }

        if (result == ERROR_SUCCESS)
        {
            DCHECK_LT(to_wchar_size(value_size_), value_.size());
            value_[to_wchar_size(value_size_)] = L'\0';
            return true;
        }
    }

    name_[0] = L'\0';
    value_[0] = L'\0';
    value_size_ = 0;

    return false;
}

// RegistryKeyIterator -----------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
RegistryKeyIterator::RegistryKeyIterator(HKEY root_key, const QString& folder_key)
{
    initialize(root_key, folder_key, 0);
}

//--------------------------------------------------------------------------------------------------
RegistryKeyIterator::RegistryKeyIterator(HKEY root_key, const QString& folder_key, REGSAM wow64access)
{
    initialize(root_key, folder_key, wow64access);
}

//--------------------------------------------------------------------------------------------------
RegistryKeyIterator::~RegistryKeyIterator()
{
    if (key_)
        RegCloseKey(key_);
}

//--------------------------------------------------------------------------------------------------
DWORD RegistryKeyIterator::subkeyCount() const
{
    DWORD count = 0;
    LONG result = RegQueryInfoKeyW(key_, nullptr, nullptr, nullptr, &count, nullptr, nullptr, nullptr,
                                   nullptr, nullptr, nullptr, nullptr);
    if (result != ERROR_SUCCESS)
        return 0;

    return count;
}

//--------------------------------------------------------------------------------------------------
bool RegistryKeyIterator::valid() const
{
    return key_ != nullptr && index_ >= 0;
}

//--------------------------------------------------------------------------------------------------
void RegistryKeyIterator::operator++()
{
    --index_;
    read();
}

//--------------------------------------------------------------------------------------------------
bool RegistryKeyIterator::read()
{
    if (valid())
    {
        DWORD ncount = ARRAYSIZE(name_);
        FILETIME written;

        LONG r = RegEnumKeyExW(key_, static_cast<DWORD>(index_), name_, &ncount, nullptr, nullptr,
                               nullptr, &written);
        if (ERROR_SUCCESS == r)
            return true;
    }

    name_[0] = '\0';
    return false;
}

//--------------------------------------------------------------------------------------------------
void RegistryKeyIterator::initialize(HKEY root_key,
                                     const QString& folder_key,
                                     REGSAM wow64access)
{
    DCHECK_EQ((wow64access & ~kWow64AccessMask), static_cast<REGSAM>(0));

    LONG result = RegOpenKeyExW(root_key, qUtf16Printable(folder_key), 0,
                                KEY_READ | wow64access, &key_);
    if (result != ERROR_SUCCESS)
    {
        key_ = nullptr;
    }
    else
    {
        DWORD count = 0;
        result = RegQueryInfoKeyW(key_, nullptr, nullptr, nullptr, &count, nullptr, nullptr, nullptr,
                                  nullptr, nullptr, nullptr, nullptr);
        if (result != ERROR_SUCCESS)
        {
            RegCloseKey(key_);
            key_ = nullptr;
        }
        else
        {
            index_ = static_cast<int>(count - 1);
        }
    }

    read();
}

} // namespace base

