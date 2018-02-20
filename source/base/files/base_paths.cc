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

// static
bool BasePaths::GetWindowsDirectory(std::experimental::filesystem::path& result)
{
    wchar_t buffer[MAX_PATH] = { 0 };

    if (!::GetWindowsDirectoryW(buffer, _countof(buffer)))
    {
        PLOG(LS_ERROR) << "GetWindowsDirectoryW failed";
        return false;
    }

    result.assign(buffer);
    return true;
}

// static
bool BasePaths::GetSystemDirectory(std::experimental::filesystem::path& result)
{
    wchar_t buffer[MAX_PATH] = { 0 };

    if (!::GetSystemDirectoryW(buffer, _countof(buffer)))
    {
        PLOG(LS_ERROR) << "GetSystemDirectoryW failed";
        return false;
    }

    result.assign(buffer);
    return true;
}

// static
bool BasePaths::GetAppDataDirectory(std::experimental::filesystem::path& result)
{
    wchar_t buffer[MAX_PATH] = { 0 };

    HRESULT hr = SHGetFolderPathW(nullptr, CSIDL_APPDATA,
                                  nullptr, SHGFP_TYPE_CURRENT, buffer);
    if (FAILED(hr))
    {
        LOG(LS_ERROR) << "SHGetFolderPathW failed: " << SystemErrorCodeToString(hr);
        return false;
    }

    result.assign(buffer);
    return true;
}

// static
bool BasePaths::GetUserDesktopDirectory(std::experimental::filesystem::path& result)
{
    wchar_t buffer[MAX_PATH] = { 0 };

    HRESULT hr = SHGetFolderPathW(nullptr, CSIDL_DESKTOPDIRECTORY,
                                  nullptr, SHGFP_TYPE_CURRENT, buffer);
    if (FAILED(hr))
    {
        LOG(LS_ERROR) << "SHGetFolderPathW failed: " << SystemErrorCodeToString(hr);
        return false;
    }

    result.assign(buffer);
    return true;
}

// static
bool BasePaths::GetUserHomeDirectory(std::experimental::filesystem::path& result)
{
    wchar_t buffer[MAX_PATH] = { 0 };

    HRESULT hr = SHGetFolderPathW(nullptr, CSIDL_PROFILE, nullptr,
                                  SHGFP_TYPE_CURRENT, buffer);
    if (FAILED(hr))
    {
        LOG(LS_ERROR) << "SHGetFolderPathW failed: " << SystemErrorCodeToString(hr);
        return false;
    }

    result.assign(buffer);
    return true;
}

// static
bool BasePaths::GetCurrentExecutableDirectory(std::experimental::filesystem::path& result)
{
    std::experimental::filesystem::path exe_path;

    if (!GetCurrentExecutableFile(exe_path))
        return false;

    result = exe_path.parent_path();
    return true;
}

// static
bool BasePaths::GetCurrentExecutableFile(std::experimental::filesystem::path& result)
{
    wchar_t buffer[MAX_PATH] = { 0 };

    if (!GetModuleFileNameW(nullptr, buffer, _countof(buffer)))
    {
        PLOG(LS_ERROR) << "GetModuleFileNameW failed";
        return false;
    }

    result.assign(buffer);
    return true;
}

} // namespace aspia
