//
// PROJECT:         Aspia
// FILE:            base/files/file_associations.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/files/file_associations.h"

#include "base/strings/string_printf.h"
#include "base/registry.h"
#include "base/logging.h"

namespace aspia {

namespace {

bool WriteKey(HKEY root_key, const WCHAR* subkey_path,
              const WCHAR* key_name, const WCHAR* key_value)
{
    RegistryKey key;

    LONG status = key.Create(root_key, subkey_path, KEY_ALL_ACCESS);
    if (status != ERROR_SUCCESS)
    {
        LOG(LS_WARNING) << "Unable to create registry key: " << subkey_path
                        << " Error: " << SystemErrorCodeToString(status);
        return false;
    }

    status = key.WriteValue(key_name, key_value);
    if (status != ERROR_SUCCESS)
    {
        LOG(LS_WARNING) << "Unable to write key value: " << SystemErrorCodeToString(status);
        return false;
    }

    return true;
}

bool DeleteKeyRecursive(HKEY root_key, const WCHAR* subkey)
{
    if (RegDeleteKeyW(root_key, subkey) == ERROR_SUCCESS)
        return true;

    for (RegistryKeyIterator iterator(root_key, subkey); iterator.Valid(); ++iterator)
    {
        std::wstring child_key_path = StringPrintf(L"%s\\%s", subkey, iterator.Name());

        if (!DeleteKeyRecursive(root_key, child_key_path.c_str()))
            return false;
    }

    return true;
}

} // namespace

// static
bool FileAssociations::Add(const Info& info)
{
    DCHECK(info.extension && info.description && info.icon_index != -1);

    if (!WriteKey(HKEY_CLASSES_ROOT, info.extension, L"", info.description))
        return false;

    std::wstring key_path = StringPrintf(L"%s\\shell\\open\\command", info.extension);
    std::wstring value = StringPrintf(L"%s \"%1\"", info.command_line.GetCommandLineString().c_str());

    if (!WriteKey(HKEY_CLASSES_ROOT, key_path.c_str(), L"", value.c_str()))
        return false;

    key_path = StringPrintf(L"%s\\DefaultIcon", info.extension);
    value = StringPrintf(L"%s,-%d", info.command_line.GetProgram().c_str(), info.icon_index);

    return WriteKey(HKEY_CLASSES_ROOT, key_path.c_str(), L"", value.c_str());
}

// static
bool FileAssociations::Remove(const WCHAR* extension)
{
    DCHECK(extension);

    return DeleteKeyRecursive(HKEY_CLASSES_ROOT, extension);
}

} // namespace aspia
