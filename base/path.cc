//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/path.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/path.h"
#include "base/unicode.h"
#include "base/logging.h"
#include <shlobj.h>

namespace aspia {

bool GetPathW(PathKey key, std::wstring* result)
{
    DCHECK(result);

    WCHAR buffer[MAX_PATH] = { 0 };

    switch (key)
    {
        case PathKey::FILE_EXE:
        {
            if (!GetModuleFileNameW(nullptr, buffer, _countof(buffer)))
                return false;
        }
        break;

        case PathKey::DIR_COMMON_APP_DATA:
        {
            if (FAILED(SHGetFolderPathW(nullptr, CSIDL_COMMON_APPDATA, nullptr,
                                       SHGFP_TYPE_CURRENT, buffer)))
                return false;
        }
        break;

        case PathKey::DIR_CURRENT:
        {
            if (!GetCurrentDirectoryW(_countof(buffer), buffer))
                return false;
        }
        break;

        case PathKey::DIR_WINDOWS:
        {
            if (!GetWindowsDirectoryW(buffer, _countof(buffer)))
                return false;
        }
        break;

        case PathKey::DIR_SYSTEM:
        {
            if (!GetSystemDirectoryW(buffer, _countof(buffer)))
                return false;
        }
        break;
    }

    result->assign(buffer);

    return true;
}

bool GetPath(PathKey key, std::string* result)
{
    DCHECK(result);

    std::wstring unicode_result;

    if (!GetPathW(key, &unicode_result))
        return false;

    *result = std::move(UTF8fromUNICODE(unicode_result));

    return true;
}

} // namespace aspia
