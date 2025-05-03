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

#include "base/win/drive_enumerator.h"

#include "base/logging.h"

#include <qt_windows.h>

namespace base {

//--------------------------------------------------------------------------------------------------
DriveEnumerator::DriveEnumerator()
{
    DWORD size = GetLogicalDriveStringsW(0, nullptr);
    if (!size)
    {
        PLOG(LS_ERROR) << "GetLogicalDriveStringsW failed";
        return;
    }

    buffer_.resize(size);

    if (!GetLogicalDriveStringsW(size, buffer_.data()))
    {
        PLOG(LS_ERROR) << "GetLogicalDriveStringsW failed";
        return;
    }

    current_ = buffer_.data();
}

//--------------------------------------------------------------------------------------------------
const DriveEnumerator::DriveInfo& DriveEnumerator::driveInfo() const
{
    drive_info_.path_.assign(current_);
    return drive_info_;
}

//--------------------------------------------------------------------------------------------------
bool DriveEnumerator::isAtEnd() const
{
    return !current_ || !current_[0];
}

//--------------------------------------------------------------------------------------------------
void DriveEnumerator::advance()
{
    current_ = wcschr(current_, 0) + 1;
}

//--------------------------------------------------------------------------------------------------
DriveEnumerator::DriveInfo::Type DriveEnumerator::DriveInfo::type() const
{
    switch (GetDriveTypeW(path_.c_str()))
    {
        case DRIVE_REMOVABLE:
            return Type::REMOVABLE;

        case DRIVE_FIXED:
            return Type::FIXED;

        case DRIVE_REMOTE:
            return Type::REMOTE;

        case DRIVE_CDROM:
            return Type::CDROM;

        case DRIVE_RAMDISK:
            return Type::RAM;

        default:
            return Type::UNKNOWN;
    }
}

//--------------------------------------------------------------------------------------------------
quint64 DriveEnumerator::DriveInfo::totalSpace() const
{
    ULARGE_INTEGER total_space;

    if (!GetDiskFreeSpaceExW(path_.c_str(), nullptr, &total_space, nullptr))
    {
        PLOG(LS_ERROR) << "GetDiskFreeSpaceExW failed";
        return 0;
    }

    return total_space.QuadPart;
}

//--------------------------------------------------------------------------------------------------
quint64 DriveEnumerator::DriveInfo::freeSpace() const
{
    ULARGE_INTEGER free_space;

    if (!GetDiskFreeSpaceExW(path_.c_str(), nullptr, nullptr, &free_space))
    {
        PLOG(LS_ERROR) << "GetDiskFreeSpaceExW failed";
        return 0;
    }

    return free_space.QuadPart;
}

//--------------------------------------------------------------------------------------------------
QString DriveEnumerator::DriveInfo::fileSystem() const
{
    wchar_t fs[MAX_PATH];

    if (!GetVolumeInformationW(path_.c_str(), nullptr,
                               0,
                               nullptr, nullptr, nullptr,
                               fs, static_cast<DWORD>(std::size(fs))))
    {
        PLOG(LS_ERROR) << "GetVolumeInformationW failed";
        return QString();
    }

    return QString::fromWCharArray(fs);
}

//--------------------------------------------------------------------------------------------------
QString DriveEnumerator::DriveInfo::volumeName() const
{
    wchar_t name[MAX_PATH];

    if (!GetVolumeInformationW(path_.c_str(), name,
                               static_cast<DWORD>(std::size(name)),
                               nullptr, nullptr, nullptr,
                               nullptr, 0))
    {
        PLOG(LS_ERROR) << "GetVolumeInformationW failed";
        return QString();
    }

    return QString::fromWCharArray(name);
}

//--------------------------------------------------------------------------------------------------
QString DriveEnumerator::DriveInfo::volumeSerial() const
{
    DWORD serial;

    if (!GetVolumeInformationW(path_.c_str(), nullptr, 0, &serial, nullptr, nullptr, nullptr, 0))
    {
        PLOG(LS_ERROR) << "GetVolumeInformationW failed";
        return QString();
    }

    return QString("%1-%2")
        .arg(HIWORD(serial), 4, 16, QLatin1Char('0'))
        .arg(LOWORD(serial), 4, 16, QLatin1Char('0')).toUpper();
}

} // namespace base
