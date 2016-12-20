/*
* PROJECT:         Aspia Remote Desktop
* FILE:            base/registry.cpp
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#include "base/registry.h"

#include "base/logging.h"

namespace aspia {

// Mask to pull WOW64 access flags out of REGSAM access.
const REGSAM kWow64AccessMask = KEY_WOW64_32KEY | KEY_WOW64_64KEY;

RegistryKey::RegistryKey() :
    key_(nullptr),
    wow64access_(0)
{
    // Nothing
}

RegistryKey::RegistryKey(HKEY key) :
    key_(key),
    wow64access_(0)
{
    // Nothing
}

RegistryKey::RegistryKey(HKEY rootkey, const WCHAR *subkey, REGSAM access) :
    key_(nullptr),
    wow64access_(0)
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

LONG RegistryKey::Create(HKEY rootkey, const WCHAR *subkey, REGSAM access)
{
    DWORD disposition_value;

    return CreateWithDisposition(rootkey, subkey, &disposition_value, access);
}

LONG RegistryKey::CreateWithDisposition(HKEY rootkey, const WCHAR *subkey,
                                        DWORD *disposition, REGSAM access)
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

LONG RegistryKey::Open(HKEY rootkey, const WCHAR *subkey, REGSAM access)
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

bool RegistryKey::HasValue(const WCHAR *name) const
{
    return (RegQueryValueExW(key_,
                             name,
                             0,
                             nullptr,
                             nullptr,
                             nullptr) == ERROR_SUCCESS);
}

LONG RegistryKey::ReadValue(const WCHAR *name,
                            void *data,
                            DWORD *dsize,
                            DWORD *dtype) const
{
    return RegQueryValueExW(key_,
                            name,
                            0,
                            dtype,
                            reinterpret_cast<LPBYTE>(data),
                            dsize);
}

LONG RegistryKey::ReadValueDW(const WCHAR *name, DWORD *out_value) const
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

LONG RegistryKey::ReadValue(const WCHAR *name, std::wstring *out_value) const
{
    DCHECK(out_value);

    const size_t kMaxStringLength = 1024;  // This is after expansion.
    // Use the one of the other forms of ReadValue if 1024 is too small for you.
    WCHAR raw_value[kMaxStringLength];
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

LONG RegistryKey::WriteValue(const WCHAR *name,
                             const void *data,
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

LONG RegistryKey::WriteValue(const WCHAR *name, DWORD in_value)
{
    return WriteValue(name,
                      &in_value,
                      static_cast<DWORD>(sizeof(in_value)),
                      REG_DWORD);
}

LONG RegistryKey::WriteValue(const WCHAR *name, const WCHAR *in_value)
{
    return WriteValue(name,
                      in_value,
                      static_cast<DWORD>(sizeof(*in_value) * (wcslen(in_value) + 1)),
                      REG_SZ);
}

} // namespace aspia

