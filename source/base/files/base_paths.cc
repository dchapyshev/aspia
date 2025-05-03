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

#include "base/files/base_paths.h"

#include "base/logging.h"

#if defined(OS_POSIX)
#include "base/environment.h"
#include <unistd.h>
#endif // defined(OS_POSIX)

#if defined(OS_LINUX)
#include "third_party/xdg_user_dirs/xdg_user_dir_lookup.h"
#include <linux/limits.h>
#endif // defined(OS_LINUX)

#if defined(OS_WIN)
#include <ShlObj.h>
#endif // defined(OS_WIN)

#if defined(OS_MAC)
#include <mach-o/dyld.h>
#include <sys/syslimits.h>
#endif // defined(OS_MAC)

namespace base {

namespace {

#if defined(OS_LINUX)
//--------------------------------------------------------------------------------------------------
std::filesystem::path xdgUserDirectory(const char* dir_name, const char* fallback_dir)
{
    std::filesystem::path path;
    char* xdg_dir = xdg_user_dir_lookup(dir_name);
    if (xdg_dir)
    {
        path = std::filesystem::path(xdg_dir);
        free(xdg_dir);
    }
    else
    {
        BasePaths::userHome(&path);
        path.append(fallback_dir);
    }
    return path;
}
#endif

} // namespace

#if defined(OS_WIN)
//--------------------------------------------------------------------------------------------------
// static
bool BasePaths::windowsDir(std::filesystem::path* result)
{
    DCHECK(result);

    wchar_t buffer[MAX_PATH] = { 0 };

    if (!GetWindowsDirectoryW(buffer, static_cast<UINT>(std::size(buffer))))
    {
        PLOG(LS_ERROR) << "GetWindowsDirectoryW failed";
        return false;
    }

    result->assign(buffer);
    return true;
}

//--------------------------------------------------------------------------------------------------
// static
bool BasePaths::systemDir(std::filesystem::path* result)
{
    DCHECK(result);

    wchar_t buffer[MAX_PATH] = { 0 };

    if (!GetSystemDirectoryW(buffer, static_cast<UINT>(std::size(buffer))))
    {
        PLOG(LS_ERROR) << "GetSystemDirectoryW failed";
        return false;
    }

    result->assign(buffer);
    return true;
}
#endif // defined(OS_WIN)

#if !defined(OS_MAC)
//--------------------------------------------------------------------------------------------------
// static
bool BasePaths::userAppData(std::filesystem::path* result)
{
    DCHECK(result);

#if defined(OS_WIN)
    wchar_t buffer[MAX_PATH] = { 0 };

    HRESULT hr = SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, buffer);
    if (FAILED(hr))
    {
        LOG(LS_ERROR) << "SHGetFolderPathW failed: " << SystemError::toString(hr);
        return false;
    }

    result->assign(buffer);
    return true;
#elif (OS_LINUX)
    std::string value;
    if (!Environment::get("XDG_DATA_HOME", &value) || value.empty())
    {
        if (!userHome(result))
            return false;

        result->append(".local/share");
        return true;
    }

    result->assign(value);
    return true;
#else
    NOTIMPLEMENTED();
    return false;
#endif
}
#endif

#if !defined(OS_MAC)
//--------------------------------------------------------------------------------------------------
// static
bool BasePaths::userDesktop(std::filesystem::path* result)
{
    DCHECK(result);

#if defined(OS_WIN)
    wchar_t buffer[MAX_PATH] = { 0 };

    HRESULT hr = SHGetFolderPathW(nullptr, CSIDL_DESKTOPDIRECTORY,
                                  nullptr, SHGFP_TYPE_CURRENT, buffer);
    if (FAILED(hr))
    {
        LOG(LS_ERROR) << "SHGetFolderPathW failed: " << SystemError::toString(hr);
        return false;
    }

    result->assign(buffer);
    return true;
#elif defined(OS_LINUX)
    *result = xdgUserDirectory("DESKTOP", "Desktop");
    return true;
#else
    NOTIMPLEMENTED();
    return false;
#endif
}
#endif

//--------------------------------------------------------------------------------------------------
// static
bool BasePaths::userHome(std::filesystem::path* result)
{
    DCHECK(result);

#if defined(OS_WIN)
    wchar_t buffer[MAX_PATH] = { 0 };

    HRESULT hr = SHGetFolderPathW(nullptr, CSIDL_PROFILE, nullptr, SHGFP_TYPE_CURRENT, buffer);
    if (FAILED(hr))
    {
        LOG(LS_ERROR) << "SHGetFolderPathW failed: " << SystemError::toString(hr);
        return false;
    }

    result->assign(buffer);
    return true;
#elif (OS_POSIX)
    std::string value;
    if (!Environment::get("HOME", &value))
        return false;

    result->assign(value);
    return true;
#else
    NOTIMPLEMENTED();
    return false;
#endif
}

#if !defined(OS_MAC)
//--------------------------------------------------------------------------------------------------
// static
bool BasePaths::commonAppData(std::filesystem::path* result)
{
    DCHECK(result);

#if defined(OS_WIN)
    wchar_t buffer[MAX_PATH] = { 0 };

    HRESULT hr = SHGetFolderPathW(nullptr, CSIDL_COMMON_APPDATA,
                                  nullptr, SHGFP_TYPE_CURRENT, buffer);
    if (FAILED(hr))
    {
        LOG(LS_ERROR) << "SHGetFolderPathW failed: " << SystemError::toString(hr);
        return false;
    }

    result->assign(buffer);
    return true;
#elif defined(OS_LINUX)
    *result = "/etc";
    return true;
#else
    NOTIMPLEMENTED();
    return false;
#endif
}
#endif

#if !defined(OS_MAC)
//--------------------------------------------------------------------------------------------------
// static
bool BasePaths::commonDesktop(std::filesystem::path* result)
{
    DCHECK(result);

#if defined(OS_WIN)
    wchar_t buffer[MAX_PATH] = { 0 };

    HRESULT hr = SHGetFolderPathW(nullptr, CSIDL_COMMON_DESKTOPDIRECTORY,
                                  nullptr, SHGFP_TYPE_CURRENT, buffer);
    if (FAILED(hr))
    {
        LOG(LS_ERROR) << "SHGetFolderPathW failed: " << SystemError::toString(hr);
        return false;
    }

    result->assign(buffer);
    return true;
#elif defined(OS_LINUX)
    return userDesktop(result);
#else
    NOTIMPLEMENTED();
    return false;
#endif
}
#endif

//--------------------------------------------------------------------------------------------------
// static
bool BasePaths::currentExecDir(std::filesystem::path* result)
{
    DCHECK(result);

    std::filesystem::path exe_path;

    if (!currentExecFile(&exe_path))
    {
        LOG(LS_ERROR) << "currentExecFile failed";
        return false;
    }

    *result = exe_path.parent_path();
    return true;
}

//--------------------------------------------------------------------------------------------------
// static
bool BasePaths::currentExecFile(std::filesystem::path* result)
{
    DCHECK(result);

#if defined(OS_WIN)
    wchar_t buffer[MAX_PATH] = { 0 };

    if (!GetModuleFileNameW(nullptr, buffer, static_cast<DWORD>(std::size(buffer))))
    {
        PLOG(LS_ERROR) << "GetModuleFileNameW failed";
        return false;
    }

    result->assign(buffer);
    return true;
#elif defined(OS_LINUX)
    char buffer[PATH_MAX] = { 0 };

    ssize_t count = readlink("/proc/self/exe", buffer, std::size(buffer));
    if (count == -1)
        return false;

    result->assign(buffer);
    return true;
#elif defined(OS_MAC)
    char buffer[PATH_MAX] = { 0 };
    quint32 buffer_size = std::size(buffer);

    if (_NSGetExecutablePath(buffer, &buffer_size) != 0)
        return false;

    result->assign(buffer);
    return true;
#else
    NOTIMPLEMENTED();
    return false;
#endif
}

} // namespace base
