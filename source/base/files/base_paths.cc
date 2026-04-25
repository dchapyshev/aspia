//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include <QDir>

#if defined(Q_OS_WINDOWS)
#include <qt_windows.h>
#include <ShlObj.h>
#endif // defined(Q_OS_WINDOWS)

#if defined(Q_OS_LINUX)
#include <linux/limits.h>
#include <unistd.h>
#endif // defined(Q_OS_LINUX)

#if defined(Q_OS_MACOS)
#include <mach-o/dyld.h>
#include <sys/syslimits.h>
#endif // defined(Q_OS_MACOS)

#include "base/logging.h"

namespace base {

//--------------------------------------------------------------------------------------------------
// static
QString BasePaths::genericConfigDir()
{
#if defined(Q_OS_WINDOWS)
    wchar_t buffer[MAX_PATH] = { 0 };
    HRESULT hr = SHGetFolderPathW(nullptr, CSIDL_COMMON_APPDATA, nullptr, SHGFP_TYPE_CURRENT, buffer);
    if (FAILED(hr))
    {
        LOG(ERROR) << "SHGetFolderPathW failed:" << SystemError::toString(hr);
        return QString();
    }

    return QDir::fromNativeSeparators(QString::fromWCharArray(buffer));
#elif defined(Q_OS_LINUX)
    return "/etc";
#elif defined(Q_OS_MACOS)
    return "/Library/Application Support";
#else
    NOTIMPLEMENTED();
    return QString();
#endif
}

//--------------------------------------------------------------------------------------------------
// static
QString BasePaths::genericUserConfigDir()
{
#if defined(Q_OS_WINDOWS)
    wchar_t buffer[MAX_PATH] = { 0 };
    HRESULT hr = SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, buffer);
    if (FAILED(hr))
    {
        LOG(ERROR) << "SHGetFolderPathW failed:" << SystemError::toString(hr);
        return QString();
    }

    return QDir::fromNativeSeparators(QString::fromWCharArray(buffer));
#elif defined(Q_OS_LINUX)
    QString path = qEnvironmentVariable("XDG_CONFIG_HOME");
    if (path.isEmpty())
    {
        path = BasePaths::userHome();
        if (path.isEmpty())
            return QString();

        return path + "/.config";
    }

    return path;
#elif defined(Q_OS_MACOS)
    QString home = BasePaths::userHome();
    if (home.isEmpty())
        return QString();

    return home + "/Library/Application Support";
#else
    NOTIMPLEMENTED();
    return QString();
#endif
}

//--------------------------------------------------------------------------------------------------
// static
QString BasePaths::appConfigDir()
{
    return genericConfigDir() + "/aspia";
}

//--------------------------------------------------------------------------------------------------
// static
QString BasePaths::appUserConfigDir()
{
    return genericUserConfigDir() + "/aspia";
}

//--------------------------------------------------------------------------------------------------
// static
QString BasePaths::genericDataDir()
{
#if defined(Q_OS_WINDOWS)
    return genericConfigDir();
#elif defined(Q_OS_LINUX)
    return "/var/lib";
#elif defined(Q_OS_MACOS)
    return "/Library/Application Support";
#else
    NOTIMPLEMENTED();
    return QString();
#endif
}

//--------------------------------------------------------------------------------------------------
// static
QString BasePaths::genericUserDataDir()
{
#if defined(Q_OS_WINDOWS)
    return genericUserConfigDir();
#elif defined(Q_OS_LINUX)
    QString path = qEnvironmentVariable("XDG_DATA_HOME");
    if (path.isEmpty())
    {
        path = BasePaths::userHome();
        if (path.isEmpty())
            return QString();

        return path + "/.local/share";
    }

    return path;
#elif defined(Q_OS_MACOS)
    QString home = BasePaths::userHome();
    if (home.isEmpty())
        return QString();

    return home + "/Library/Application Support";
#else
    NOTIMPLEMENTED();
    return QString();
#endif
}

//--------------------------------------------------------------------------------------------------
// static
QString BasePaths::appDataDir()
{
    return genericDataDir() + "/aspia";
}

//--------------------------------------------------------------------------------------------------
// static
QString BasePaths::appUserDataDir()
{
    return genericUserDataDir() + "/aspia";
}

//--------------------------------------------------------------------------------------------------
// static
QString BasePaths::userHome()
{
#if defined(Q_OS_WINDOWS)
    wchar_t buffer[MAX_PATH] = { 0 };
    HRESULT hr = SHGetFolderPathW(nullptr, CSIDL_PROFILE, nullptr, SHGFP_TYPE_CURRENT, buffer);
    if (FAILED(hr))
    {
        LOG(ERROR) << "SHGetFolderPathW failed: " << SystemError::toString(hr);
        return QString();
    }

    return QDir::fromNativeSeparators(QString::fromWCharArray(buffer));
#elif defined(Q_OS_POSIX)
    return qEnvironmentVariable("HOME");
#else
    NOTIMPLEMENTED();
    return QString();
#endif
}

//--------------------------------------------------------------------------------------------------
// static
QString BasePaths::currentApp()
{
#if defined(Q_OS_WINDOWS)
    wchar_t buffer[MAX_PATH] = { 0 };
    if (!GetModuleFileNameW(nullptr, buffer, static_cast<DWORD>(std::size(buffer))))
    {
        PLOG(ERROR) << "GetModuleFileNameW failed";
        return QString();
    }

    return QDir::fromNativeSeparators(QString::fromWCharArray(buffer));
#elif defined(Q_OS_LINUX)
    char buffer[PATH_MAX] = { 0 };
    ssize_t count = readlink("/proc/self/exe", buffer, std::size(buffer));
    if (count == -1)
        return QString();

    return QString::fromLocal8Bit(buffer);
#elif defined(Q_OS_MACOS)
    char buffer[PATH_MAX] = { 0 };
    uint32_t buffer_size = std::size(buffer);
    if (_NSGetExecutablePath(buffer, &buffer_size) != 0)
        return QString();

    return QString::fromLocal8Bit(buffer);
#else
    NOTIMPLEMENTED();
    return QString();
#endif
}

} // namespace base
