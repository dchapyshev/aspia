//
// PROJECT:         Aspia
// FILE:            base/files/file_associations.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/files/file_associations.h"

#include <algorithm>
#include <cwctype>
#include <shlobj.h>

#include "base/strings/string_printf.h"
#include "base/registry.h"
#include "base/logging.h"

namespace aspia {

namespace {

std::wstring EraseSpaces(std::wstring_view str)
{
    std::wstring nospace_str(str);
    nospace_str.erase(std::remove_if(nospace_str.begin(), nospace_str.end(), std::iswspace));
    return nospace_str;
}

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
bool FileAssociations::Add(const CommandLine& command_line,
                           const WCHAR* extension,
                           const WCHAR* description,
                           int icon_index)
{
    DCHECK(extension && description && icon_index != -1);

    if (WriteKey(HKEY_CLASSES_ROOT, extension, L"", description))
    {
        std::wstring key_name = EraseSpaces(description);
        std::wstring key_path = StringPrintf(L"%s\\shell\\open\\command", key_name.c_str());
        std::wstring value = StringPrintf(L"%s \"%1\"", command_line.GetCommandLineString().c_str());

        if (WriteKey(HKEY_CLASSES_ROOT, key_path.c_str(), L"", value.c_str()))
        {
            key_path = StringPrintf(L"%s\\DefaultIcon", key_name.c_str());
            value = StringPrintf(L"%s,-%d", command_line.GetProgram().c_str(), icon_index);

            if (WriteKey(HKEY_CLASSES_ROOT, key_path.c_str(), L"", value.c_str()))
            {
                SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
                return true;
            }

            DeleteKeyRecursive(HKEY_CLASSES_ROOT, key_path.c_str());
        }

        DeleteKeyRecursive(HKEY_CLASSES_ROOT, extension);
    }

    return false;
}

// static
bool FileAssociations::Remove(const WCHAR* extension)
{
    DCHECK(extension);

    if (!DeleteKeyRecursive(HKEY_CLASSES_ROOT, extension))
        return false;

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    return true;
}

} // namespace aspia
