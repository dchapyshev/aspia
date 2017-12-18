//
// PROJECT:         Aspia
// FILE:            base/files/logical_drive_enumerator.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/files/logical_drive_enumerator.h"
#include "base/strings/string_printf.h"
#include "base/logging.h"

namespace aspia {

LogicalDriveEnumerator::LogicalDriveEnumerator()
{
    DWORD size = GetLogicalDriveStringsW(0, nullptr);
    if (!size)
    {
        LOG(ERROR) << "GetLogicalDriveStringsW() failed: " << GetLastSystemErrorString();
        return;
    }

    buffer_.resize(size);

    if (!GetLogicalDriveStringsW(size, buffer_.data()))
    {
        LOG(ERROR) << "GetLogicalDriveStringsW() failed: " << GetLastSystemErrorString();
        return;
    }

    next_ = buffer_.data();
}

FilePath LogicalDriveEnumerator::Next()
{
    if (!next_ || !next_[0])
        return std::wstring();

    current_.assign(next_);

    next_ = wcschr(next_, 0) + 1;

    return current_;
}

LogicalDriveEnumerator::DriveInfo LogicalDriveEnumerator::GetInfo() const
{
    return DriveInfo(current_);
}

LogicalDriveEnumerator::DriveInfo::DriveInfo(const FilePath& path) :
    path_(path)
{
    // Nothing
}

FilePath LogicalDriveEnumerator::DriveInfo::Path() const
{
    return path_;
}

LogicalDriveEnumerator::DriveInfo::DriveType LogicalDriveEnumerator::DriveInfo::Type() const
{
    switch (GetDriveTypeW(path_.c_str()))
    {
        case DRIVE_REMOVABLE:
            return DriveType::REMOVABLE;

        case DRIVE_FIXED:
            return DriveType::FIXED;

        case DRIVE_REMOTE:
            return DriveType::REMOTE;

        case DRIVE_CDROM:
            return DriveType::CDROM;

        case DRIVE_RAMDISK:
            return DriveType::RAM;

        default:
            return DriveType::UNKNOWN;
    }
}

uint64_t LogicalDriveEnumerator::DriveInfo::TotalSpace() const
{
    ULARGE_INTEGER total_space;

    if (!GetDiskFreeSpaceExW(path_.c_str(), nullptr, &total_space, nullptr))
    {
        DLOG(ERROR) << "GetDiskFreeSpaceExW() failed: " << GetLastSystemErrorString();
        return 0;
    }

    return total_space.QuadPart;
}

uint64_t LogicalDriveEnumerator::DriveInfo::FreeSpace() const
{
    ULARGE_INTEGER free_space;

    if (!GetDiskFreeSpaceExW(path_.c_str(), nullptr, nullptr, &free_space))
    {
        DLOG(ERROR) << "GetDiskFreeSpaceExW() failed: " << GetLastSystemErrorString();
        return 0;
    }

    return free_space.QuadPart;
}

std::wstring LogicalDriveEnumerator::DriveInfo::FileSystem() const
{
    wchar_t fs[MAX_PATH];

    if (!GetVolumeInformationW(path_.c_str(), nullptr,
                               0,
                               nullptr, nullptr, nullptr,
                               fs, _countof(fs)))
    {
        DLOG(ERROR) << "GetVolumeInformationW() failed: " << GetLastSystemErrorString();
        return std::wstring();
    }

    return fs;
}

std::wstring LogicalDriveEnumerator::DriveInfo::VolumeName() const
{
    wchar_t name[MAX_PATH];

    if (!GetVolumeInformationW(path_.c_str(), name,
                               _countof(name),
                               nullptr, nullptr, nullptr,
                               nullptr, 0))
    {
        DLOG(ERROR) << "GetVolumeInformationW() failed: " << GetLastSystemErrorString();
        return std::wstring();
    }

    return name;
}

std::string LogicalDriveEnumerator::DriveInfo::VolumeSerial() const
{
    DWORD serial;

    if (!GetVolumeInformationW(path_.c_str(), nullptr, 0, &serial, nullptr, nullptr, nullptr, 0))
    {
        DLOG(ERROR) << "GetVolumeInformationW() failed: " << GetLastSystemErrorString();
        return std::string();
    }

    return StringPrintf("%04X-%04X", HIWORD(serial), LOWORD(serial));
}

} // namespace aspia
