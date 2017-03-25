//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/path.cpp
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/path.h"
#include "base/unicode.h"
#include "base/logging.h"
#include <shlobj.h>

namespace aspia {

bool GetPath(PathKey key, std::wstring* result)
{
    DCHECK(result);

    WCHAR buffer[MAX_PATH] = { 0 };

    switch (key)
    {
        case PathKey::FILE_EXE:
        {
            if (!GetModuleFileNameW(nullptr, buffer, ARRAYSIZE(buffer)))
                return false;
        }
        break;

        case PathKey::DIR_CURRENT:
        {
            if (!GetCurrentDirectoryW(ARRAYSIZE(buffer), buffer))
                return false;
        }
        break;

        case PathKey::DIR_WINDOWS:
        {
            if (!GetWindowsDirectoryW(buffer, ARRAYSIZE(buffer)))
                return false;
        }
        break;

        case PathKey::DIR_SYSTEM:
        {
            if (!GetSystemDirectoryW(buffer, ARRAYSIZE(buffer)))
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

    if (!GetPath(key, &unicode_result))
        return false;

    *result = std::move(UTF8fromUNICODE(unicode_result));

    return true;
}

} // namespace aspia
