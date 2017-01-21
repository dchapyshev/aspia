//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/registry.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__REGISTRY_H
#define _ASPIA_BASE__REGISTRY_H

#include "aspia_config.h"

#include <string>

#include "base/macros.h"

namespace aspia {

class RegistryKey
{
public:
    RegistryKey();
    explicit RegistryKey(HKEY key);
    RegistryKey(HKEY rootkey, const WCHAR *subkey, REGSAM access);

    ~RegistryKey();

    // True while the key is valid.
    bool IsValid() const;

    LONG Create(HKEY rootkey, const WCHAR *subkey, REGSAM access);

    LONG CreateWithDisposition(HKEY rootkey, const WCHAR *subkey,
                               DWORD *disposition, REGSAM access);

    // Opens an existing reg key.
    LONG Open(HKEY rootkey, const WCHAR *subkey, REGSAM access);

    //
    // Returns false if this key does not have the specified value, or if an error
    // occurrs while attempting to access it.
    //
    bool HasValue(const WCHAR *name) const;

    //
    // Reads raw data into |data|. If |name| is null or empty, reads the key's
    // default value, if any.
    //
    LONG ReadValue(const WCHAR *name, void *data, DWORD *dsize, DWORD *dtype) const;

    //
    // Reads a REG_DWORD (uint32_t) into |out_value|. If |name| is null or empty,
    // reads the key's default value, if any.
    //
    LONG ReadValueDW(const WCHAR *name, DWORD *out_value) const;

    //
    // Reads a string into |out_value|. If |name| is null or empty, reads
    // the key's default value, if any.
    //
    LONG ReadValue(const WCHAR *name, std::wstring *out_value) const;

    // Sets raw data, including type.
    LONG WriteValue(const WCHAR *name, const void *data, DWORD dsize, DWORD dtype);

    // Sets an int32_t value.
    LONG WriteValue(const WCHAR *name, DWORD in_value);

    // Sets a string value.
    LONG WriteValue(const WCHAR *name, const WCHAR *in_value);

    // Closes this reg key.
    void Close();

private:
    HKEY key_;
    REGSAM wow64access_;

    DISALLOW_COPY_AND_ASSIGN(RegistryKey);
};

} // namespace aspia

#endif // _ASPIA_BASE__REGISTRY_H
