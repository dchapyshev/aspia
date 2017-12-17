//
// PROJECT:         Aspia
// FILE:            base/files/base_paths.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/files/base_paths.h"
#include "base/logging.h"

#include <shlobj.h>

namespace aspia {

bool GetBasePath(BasePathKey key, FilePath& result)
{
    WCHAR buffer[MAX_PATH] = { 0 };

    switch (key)
    {
        case BasePathKey::FILE_EXE:
        {
            if (!GetModuleFileNameW(nullptr, buffer, _countof(buffer)))
            {
                LOG(ERROR) << "GetModuleFileNameW() failed: "
                           << GetLastSystemErrorString();
                return false;
            }
        }
        break;

        case BasePathKey::DIR_COMMON_APP_DATA:
        {
            HRESULT hr = SHGetFolderPathW(nullptr, CSIDL_COMMON_APPDATA,
                                          nullptr, SHGFP_TYPE_CURRENT, buffer);
            if (FAILED(hr))
            {
                LOG(ERROR) << "SHGetFolderPathW() failed: "
                           << SystemErrorCodeToString(hr);
                return false;
            }
        }
        break;

        case BasePathKey::DIR_COMMON_DESKTOP:
        {
            HRESULT hr = SHGetFolderPathW(nullptr, CSIDL_COMMON_DESKTOPDIRECTORY,
                                          nullptr, SHGFP_TYPE_CURRENT, buffer);
            if (FAILED(hr))
            {
                LOG(ERROR) << "SHGetFolderPathW() failed: "
                           << SystemErrorCodeToString(hr);
                return false;
            }
        }
        break;

        case BasePathKey::DIR_USER_DESKTOP:
        {
            HRESULT hr = SHGetFolderPathW(nullptr, CSIDL_DESKTOPDIRECTORY,
                                          nullptr, SHGFP_TYPE_CURRENT, buffer);
            if (FAILED(hr))
            {
                LOG(ERROR) << "SHGetFolderPathW() failed: "
                           << SystemErrorCodeToString(hr);
                return false;
            }
        }
        break;

        case BasePathKey::DIR_USER_HOME:
        {
            HRESULT hr = SHGetFolderPathW(nullptr, CSIDL_PROFILE, nullptr,
                                          SHGFP_TYPE_CURRENT, buffer);
            if (FAILED(hr))
            {
                LOG(ERROR) << "SHGetFolderPathW() failed: "
                           << SystemErrorCodeToString(hr);
                return false;
            }
        }
        break;

        case BasePathKey::DIR_CURRENT:
        {
            if (!GetCurrentDirectoryW(_countof(buffer), buffer))
            {
                LOG(ERROR) << "GetCurrentDirectoryW() failed: "
                           << GetLastSystemErrorString();
                return false;
            }
        }
        break;

        case BasePathKey::DIR_WINDOWS:
        {
            if (!GetWindowsDirectoryW(buffer, _countof(buffer)))
            {
                LOG(ERROR) << "GetWindowsDirectoryW() failed: "
                           << GetLastSystemErrorString();
                return false;
            }
        }
        break;

        case BasePathKey::DIR_SYSTEM:
        {
            if (!GetSystemDirectoryW(buffer, _countof(buffer)))
            {
                LOG(ERROR) << "GetSystemDirectoryW() failed: "
                           << GetLastSystemErrorString();
                return false;
            }
        }
        break;
    }

    result.assign(buffer);

    return true;
}

} // namespace aspia
