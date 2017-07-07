//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/registry.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/registry.h"

#include "base/logging.h"

namespace aspia {

// RegEnumValue() reports the number of characters from the name that were
// written to the buffer, not how many there are. This constant is the maximum
// name size, such that a buffer with this size should read any name.
const DWORD MAX_REGISTRY_NAME_SIZE = 16384;

// Registry values are read as BYTE* but can have wchar_t* data whose last
// wchar_t is truncated. This function converts the reported |byte_size| to
// a size in wchar_t that can store a truncated wchar_t if necessary.
INLINE DWORD to_wchar_size(DWORD byte_size)
{
    return (byte_size + sizeof(WCHAR) - 1) / sizeof(WCHAR);
}

// Mask to pull WOW64 access flags out of REGSAM access.
const REGSAM kWow64AccessMask = KEY_WOW64_32KEY | KEY_WOW64_64KEY;

WCHAR* WriteInto(std::wstring* str, size_t length_with_null)
{
    DCHECK_GT(length_with_null, 1u);
    str->reserve(length_with_null);
    str->resize(length_with_null - 1);
    return &((*str)[0]);
}

RegistryKey::RegistryKey(HKEY key) :
    key_(key)
{
    // Nothing
}

RegistryKey::RegistryKey(HKEY rootkey, const WCHAR *subkey, REGSAM access)
{
    if (rootkey)
    {
        if (access & (KEY_SET_VALUE | KEY_CREATE_SUB_KEY | KEY_CREATE_LINK))
            Create(rootkey, subkey, access);
        else
            Open(rootkey, subkey, access);
    }
    else
    {
        DCHECK(!subkey);
        wow64access_ = access & kWow64AccessMask;
    }
}

RegistryKey::~RegistryKey()
{
    Close();
}

bool RegistryKey::IsValid() const
{
    return (key_ != nullptr);
}

LONG RegistryKey::Create(HKEY rootkey, const WCHAR* subkey, REGSAM access)
{
    DWORD disposition_value;

    return CreateWithDisposition(rootkey, subkey, &disposition_value, access);
}

LONG RegistryKey::CreateWithDisposition(HKEY rootkey, const WCHAR* subkey,
                                        DWORD* disposition, REGSAM access)
{
    DCHECK(rootkey && subkey && access && disposition);

    HKEY subhkey = nullptr;

    LONG result = RegCreateKeyExW(rootkey,
                                  subkey,
                                  0,
                                  nullptr,
                                  REG_OPTION_NON_VOLATILE,
                                  access,
                                  nullptr,
                                  &subhkey,
                                  disposition);
    if (result == ERROR_SUCCESS)
    {
        Close();
        key_ = subhkey;
        wow64access_ = access & kWow64AccessMask;
    }

    return result;
}

LONG RegistryKey::Open(HKEY rootkey, const WCHAR* subkey, REGSAM access)
{
    DCHECK(rootkey && subkey && access);

    HKEY subhkey = nullptr;

    LONG result = RegOpenKeyExW(rootkey, subkey, 0, access, &subhkey);
    if (result == ERROR_SUCCESS)
    {
        Close();
        key_ = subhkey;
        wow64access_ = access & kWow64AccessMask;
    }

    return result;
}

void RegistryKey::Close()
{
    if (key_)
    {
        RegCloseKey(key_);
        key_ = nullptr;
        wow64access_ = 0;
    }
}

bool RegistryKey::HasValue(const WCHAR* name) const
{
    return (RegQueryValueExW(key_,
                             name,
                             0,
                             nullptr,
                             nullptr,
                             nullptr) == ERROR_SUCCESS);
}

LONG RegistryKey::ReadValue(const WCHAR* name,
                            void* data,
                            DWORD* dsize,
                            DWORD* dtype) const
{
    return RegQueryValueExW(key_,
                            name,
                            0,
                            dtype,
                            reinterpret_cast<LPBYTE>(data),
                            dsize);
}

LONG RegistryKey::ReadValueDW(const WCHAR* name, DWORD* out_value) const
{
    DCHECK(out_value);

    DWORD type = REG_DWORD;
    DWORD size = sizeof(DWORD);
    DWORD local_value = 0;

    LONG result = ReadValue(name, &local_value, &size, &type);

    if (result == ERROR_SUCCESS)
    {
        if ((type == REG_DWORD || type == REG_BINARY) && size == sizeof(DWORD))
            *out_value = local_value;
        else
            result = ERROR_CANTREAD;
    }

    return result;
}

LONG RegistryKey::ReadValue(const WCHAR* name, std::wstring* out_value) const
{
    DCHECK(out_value);

    const size_t kMaxStringLength = 1024;  // This is after expansion.
    // Use the one of the other forms of ReadValue if 1024 is too small for you.
    WCHAR raw_value[kMaxStringLength] = { 0 };
    DWORD type = REG_SZ;
    DWORD size = sizeof(raw_value);

    LONG result = ReadValue(name, raw_value, &size, &type);
    if (result == ERROR_SUCCESS)
    {
        if (type == REG_SZ)
        {
            *out_value = raw_value;
        }
        else if (type == REG_EXPAND_SZ)
        {
            WCHAR expanded[kMaxStringLength];

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

LONG RegistryKey::WriteValue(const WCHAR* name,
                             const void* data,
                             DWORD dsize,
                             DWORD dtype)
{
    DCHECK(data || !dsize);

    return RegSetValueExW(key_,
                          name,
                          0,
                          dtype,
                          reinterpret_cast<LPBYTE>(const_cast<void*>(data)),
                          dsize);
}

LONG RegistryKey::WriteValue(const WCHAR* name, DWORD in_value)
{
    return WriteValue(name,
                      &in_value,
                      static_cast<DWORD>(sizeof(in_value)),
                      REG_DWORD);
}

LONG RegistryKey::WriteValue(const WCHAR* name, const WCHAR* in_value)
{
    return WriteValue(name,
                      in_value,
                      static_cast<DWORD>(sizeof(*in_value) * (wcslen(in_value) + 1)),
                      REG_SZ);
}

// RegistryValueIterator ------------------------------------------------------

RegistryValueIterator::RegistryValueIterator(HKEY root_key, const WCHAR* folder_key, REGSAM wow64access) :
    name_(MAX_PATH, L'\0'),
    value_(MAX_PATH, L'\0')
{
    Initialize(root_key, folder_key, wow64access);
}

RegistryValueIterator::RegistryValueIterator(HKEY root_key, const WCHAR* folder_key) :
    name_(MAX_PATH, L'\0'),
    value_(MAX_PATH, L'\0')
{
    Initialize(root_key, folder_key, 0);
}

void RegistryValueIterator::Initialize(HKEY root_key, const WCHAR* folder_key, REGSAM wow64access)
{
    DCHECK_EQ(wow64access & ~kWow64AccessMask, static_cast<REGSAM>(0));

    LONG result = RegOpenKeyExW(root_key, folder_key, 0, KEY_READ | wow64access, &key_);
    if (result != ERROR_SUCCESS)
    {
        key_ = nullptr;
    }
    else
    {
        DWORD count = 0;
        result = RegQueryInfoKeyW(key_, nullptr, 0, nullptr, nullptr, nullptr, nullptr, &count,
                                  nullptr, nullptr, nullptr, nullptr);

        if (result != ERROR_SUCCESS)
        {
            RegCloseKey(key_);
            key_ = nullptr;
        }
        else
        {
            index_ = count - 1;
        }
    }

    Read();
}

RegistryValueIterator::~RegistryValueIterator()
{
    if (key_)
        RegCloseKey(key_);
}

DWORD RegistryValueIterator::ValueCount() const
{
    DWORD count = 0;
    LONG result = RegQueryInfoKeyW(key_, nullptr, 0, nullptr, nullptr, nullptr, nullptr,
                                   &count, nullptr, nullptr, nullptr, nullptr);
    if (result != ERROR_SUCCESS)
        return 0;

    return count;
}

bool RegistryValueIterator::Valid() const
{
    return key_ != nullptr && index_ >= 0;
}

void RegistryValueIterator::operator++()
{
    --index_;
    Read();
}

bool RegistryValueIterator::Read()
{
    if (Valid())
    {
        DWORD capacity = static_cast<DWORD>(name_.capacity());
        DWORD name_size = capacity;

        // |value_size_| is in bytes. Reserve the last character for a NUL.
        value_size_ = static_cast<DWORD>((value_.size() - 1) * sizeof(WCHAR));

        LONG result = RegEnumValueW(key_, index_, WriteInto(&name_, name_size), &name_size, NULL, &type_,
                                    reinterpret_cast<BYTE*>(value_.data()), &value_size_);

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

            value_size_ = static_cast<DWORD>((value_.size() - 1) * sizeof(WCHAR));
            name_size = name_size == capacity ? MAX_REGISTRY_NAME_SIZE : capacity;

            result = RegEnumValueW(key_, index_, WriteInto(&name_, name_size), &name_size, NULL, &type_,
                                   reinterpret_cast<BYTE*>(value_.data()), &value_size_);
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

// RegistryKeyIterator --------------------------------------------------------

RegistryKeyIterator::RegistryKeyIterator(HKEY root_key, const WCHAR* folder_key)
{
    Initialize(root_key, folder_key, 0);
}

RegistryKeyIterator::RegistryKeyIterator(HKEY root_key, const WCHAR* folder_key, REGSAM wow64access)
{
    Initialize(root_key, folder_key, wow64access);
}

RegistryKeyIterator::~RegistryKeyIterator()
{
    if (key_)
        RegCloseKey(key_);
}

DWORD RegistryKeyIterator::SubkeyCount() const
{
    DWORD count = 0;
    LONG result = RegQueryInfoKeyW(key_, nullptr, 0, nullptr, &count, nullptr, nullptr,
                                   nullptr, nullptr, nullptr, nullptr, nullptr);
    if (result != ERROR_SUCCESS)
        return 0;

    return count;
}

bool RegistryKeyIterator::Valid() const
{
    return key_ != nullptr && index_ >= 0;
}

void RegistryKeyIterator::operator++()
{
    --index_;
    Read();
}

bool RegistryKeyIterator::Read()
{
    if (Valid())
    {
        DWORD ncount = ARRAYSIZE(name_);
        FILETIME written;

        LONG r = RegEnumKeyExW(key_, index_, name_, &ncount, nullptr, nullptr, nullptr, &written);
        if (ERROR_SUCCESS == r)
            return true;
    }

    name_[0] = '\0';
    return false;
}

void RegistryKeyIterator::Initialize(HKEY root_key, const WCHAR* folder_key, REGSAM wow64access)
{
    DCHECK_EQ(wow64access & ~kWow64AccessMask, static_cast<REGSAM>(0));

    LONG result = RegOpenKeyExW(root_key, folder_key, 0, KEY_READ | wow64access, &key_);
    if (result != ERROR_SUCCESS)
    {
        key_ = nullptr;
    }
    else
    {
        DWORD count = 0;
        result = RegQueryInfoKeyW(key_, nullptr, 0, nullptr, &count, nullptr, nullptr, nullptr,
                                  nullptr, nullptr, nullptr, nullptr);

        if (result != ERROR_SUCCESS)
        {
            RegCloseKey(key_);
            key_ = nullptr;
        }
        else
        {
            index_ = count - 1;
        }
    }

    Read();
}

} // namespace aspia

