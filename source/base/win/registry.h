//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef BASE_WIN_REGISTRY_H
#define BASE_WIN_REGISTRY_H

#include "base/macros_magic.h"

#include <string>
#include <vector>

#include <qt_windows.h>

namespace base {

class RegistryKey
{
public:
    RegistryKey() = default;
    explicit RegistryKey(HKEY key);
    RegistryKey(HKEY rootkey, const wchar_t* subkey, REGSAM access);

    RegistryKey(RegistryKey&& other) noexcept;
    RegistryKey& operator=(RegistryKey&& other) noexcept;

    ~RegistryKey();

    // True while the key is valid.
    bool isValid() const;

    LONG create(HKEY rootkey, const wchar_t* subkey, REGSAM access);

    LONG createWithDisposition(HKEY rootkey, const wchar_t* subkey,
                               DWORD *disposition, REGSAM access);

    // Creates a subkey or open it if it already exists.
    LONG createKey(const wchar_t* name, REGSAM access);

    // Opens an existing reg key.
    LONG open(HKEY rootkey, const wchar_t* subkey, REGSAM access);

    // Opens an existing reg key, given the relative key name.
    LONG openKey(const wchar_t* relative_key_name, REGSAM access);

    // Returns false if this key does not have the specified value, or if an error
    // occurrs while attempting to access it.
    bool hasValue(const wchar_t* name) const;

    // Returns the number of values for this key, or 0 if the number cannot be determined.
    DWORD valueCount() const;

    // Reads raw data into |data|. If |name| is null or empty, reads the key's default value, if any.
    LONG readValue(const wchar_t* name, void* data, DWORD* dsize, DWORD* dtype) const;

    // Reads a REG_DWORD (uint32_t) into |out_value|. If |name| is null or empty, reads the key's
    // default value, if any.
    LONG readValueDW(const wchar_t* name, DWORD* out_value) const;

    // Reads a REG_QWORD (int64_t) into |out_value|. If |name| is null or empty, reads the key's
    // default value, if any.
    LONG readInt64(const wchar_t* name, int64_t* out_value) const;

    // Reads a REG_BINARY (array of chars) into |out_value|. If |name| is null or empty,
    // reads the key's default value, if any.
    LONG readValueBIN(const wchar_t* name, std::string* out_value) const;

    // Reads a string into |out_value|. If |name| is null or empty, reads
    // the key's default value, if any.
    LONG readValue(const wchar_t* name, std::wstring* out_value) const;

    // Sets raw data, including type.
    LONG writeValue(const wchar_t* name, const void* data, DWORD dsize, DWORD dtype);

    // Sets an int32_t value.
    LONG writeValue(const wchar_t* name, DWORD in_value);

    // Sets a string value.
    LONG writeValue(const wchar_t* name, const wchar_t* in_value);

    // Kills a key and everything that lives below it; please be careful when using it.
    LONG deleteKey(const wchar_t* name);

    // Deletes an empty subkey.  If the subkey has subkeys or values then this will fail.
    LONG deleteEmptyKey(const wchar_t* name);

    // Deletes a single value within the key.
    LONG deleteValue(const wchar_t* name);

    // Closes this reg key.
    void close();

private:
    // Recursively deletes a key and all of its subkeys.
    static LONG deleteKeyRecurse(HKEY root_key, const std::wstring& name, REGSAM access);

    HKEY key_ = nullptr;
    REGSAM wow64access_ = 0;

    DISALLOW_COPY_AND_ASSIGN(RegistryKey);
};

// Iterates the entries found in a particular folder on the registry.
class RegistryValueIterator
{
public:
    // Constructs a Registry Value Iterator with default WOW64 access.
    RegistryValueIterator(HKEY root_key, const wchar_t* folder_key);

    // Constructs a Registry Key Iterator with specific WOW64 access, one of
    // KEY_WOW64_32KEY or KEY_WOW64_64KEY, or 0.
    // Note: |wow64access| should be the same access used to open |root_key|
    // previously, or a predefined key (e.g. HKEY_LOCAL_MACHINE).
    // See http://msdn.microsoft.com/en-us/library/windows/desktop/aa384129.aspx.
    RegistryValueIterator(HKEY root_key,
                          const wchar_t* folder_key,
                          REGSAM wow64access);

    ~RegistryValueIterator();

    DWORD valueCount() const;

    // True while the iterator is valid.
    bool valid() const;

    // Advances to the next registry entry.
    void operator++();

    const wchar_t* name() const { return name_.c_str(); }
    const wchar_t* value() const { return value_.data(); }
    // ValueSize() is in bytes.
    DWORD valueSize() const { return value_size_; }
    DWORD type() const { return type_; }

    int index() const { return index_; }

private:
    // Reads in the current values.
    bool read();

    void initialize(HKEY root_key, const wchar_t* folder_key, REGSAM wow64access);

    // The registry key being iterated.
    HKEY key_ = nullptr;

    // Current index of the iteration.
    int index_;

    // Current values.
    std::wstring name_;
    std::vector<wchar_t> value_;
    DWORD value_size_;
    DWORD type_;

    DISALLOW_COPY_AND_ASSIGN(RegistryValueIterator);
};

class RegistryKeyIterator
{
public:
    // Constructs a Registry Key Iterator with default WOW64 access.
    RegistryKeyIterator(HKEY root_key, const wchar_t* folder_key);

    // Constructs a Registry Value Iterator with specific WOW64 access, one of
    // KEY_WOW64_32KEY or KEY_WOW64_64KEY, or 0.
    // Note: |wow64access| should be the same access used to open |root_key|
    // previously, or a predefined key (e.g. HKEY_LOCAL_MACHINE).
    // See http://msdn.microsoft.com/en-us/library/windows/desktop/aa384129.aspx.
    RegistryKeyIterator(HKEY root_key, const wchar_t* folder_key, REGSAM wow64access);

    ~RegistryKeyIterator();

    DWORD subkeyCount() const;

    // True while the iterator is valid.
    bool valid() const;

    // Advances to the next entry in the folder.
    void operator++();

    const wchar_t* name() const { return name_; }

    int index() const { return index_; }

private:
    // Reads in the current values.
    bool read();

    void initialize(HKEY root_key, const wchar_t* folder_key, REGSAM wow64access);

    // The registry key being iterated.
    HKEY key_ = nullptr;

    // Current index of the iteration.
    int index_;

    wchar_t name_[MAX_PATH];

    DISALLOW_COPY_AND_ASSIGN(RegistryKeyIterator);
};

} // namespace base

#endif // BASE_WIN_REGISTRY_H
