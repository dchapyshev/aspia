//
// PROJECT:         Aspia
// FILE:            base/files/base_paths.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/files/base_paths.h"
#include "base/logging.h"

#include <shlobj.h>

namespace aspia {

// static
std::optional<std::experimental::filesystem::path> BasePaths::GetWindowsDirectory()
{
    wchar_t buffer[MAX_PATH] = { 0 };

    if (!::GetWindowsDirectoryW(buffer, _countof(buffer)))
    {
        PLOG(LS_ERROR) << "GetWindowsDirectoryW failed";
        return {};
    }

    return buffer;
}

// static
std::optional<std::experimental::filesystem::path> BasePaths::GetSystemDirectory()
{
    wchar_t buffer[MAX_PATH] = { 0 };

    if (!::GetSystemDirectoryW(buffer, _countof(buffer)))
    {
        PLOG(LS_ERROR) << "GetSystemDirectoryW failed";
        return {};
    }

    return buffer;
}

// static
std::optional<std::experimental::filesystem::path> BasePaths::GetAppDataDirectory()
{
    wchar_t buffer[MAX_PATH] = { 0 };

    HRESULT hr = SHGetFolderPathW(nullptr, CSIDL_APPDATA,
                                  nullptr, SHGFP_TYPE_CURRENT, buffer);
    if (FAILED(hr))
    {
        LOG(LS_ERROR) << "SHGetFolderPathW failed: " << SystemErrorCodeToString(hr);
        return {};
    }

    return buffer;
}

// static
std::optional<std::experimental::filesystem::path> BasePaths::GetUserDesktopDirectory()
{
    wchar_t buffer[MAX_PATH] = { 0 };

    HRESULT hr = SHGetFolderPathW(nullptr, CSIDL_DESKTOPDIRECTORY,
                                  nullptr, SHGFP_TYPE_CURRENT, buffer);
    if (FAILED(hr))
    {
        LOG(LS_ERROR) << "SHGetFolderPathW failed: " << SystemErrorCodeToString(hr);
        return {};
    }

    return buffer;
}

// static
std::optional<std::experimental::filesystem::path> BasePaths::GetUserHomeDirectory()
{
    wchar_t buffer[MAX_PATH] = { 0 };

    HRESULT hr = SHGetFolderPathW(nullptr, CSIDL_PROFILE, nullptr,
                                  SHGFP_TYPE_CURRENT, buffer);
    if (FAILED(hr))
    {
        LOG(LS_ERROR) << "SHGetFolderPathW failed: " << SystemErrorCodeToString(hr);
        return {};
    }

    return buffer;
}

// static
std::optional<std::experimental::filesystem::path> BasePaths::GetCurrentExecutableDirectory()
{
    auto exe_path = GetCurrentExecutableFile();
    if (!exe_path.has_value())
        return {};

    return exe_path->parent_path();
}

// static
std::optional<std::experimental::filesystem::path> BasePaths::GetCurrentExecutableFile()
{
    wchar_t buffer[MAX_PATH] = { 0 };

    if (!GetModuleFileNameW(nullptr, buffer, _countof(buffer)))
    {
        PLOG(LS_ERROR) << "GetModuleFileNameW failed";
        return {};
    }

    return buffer;
}

} // namespace aspia
