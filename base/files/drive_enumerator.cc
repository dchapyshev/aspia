//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/files/drive_enumerator.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/files/drive_enumerator.h"
#include "base/logging.h"

namespace aspia {

DriveEnumerator::DriveEnumerator()
{
    DWORD size = GetLogicalDriveStringsW(0, nullptr);
    if (!size)
    {
        LOG(ERROR) << "GetLogicalDriveStringsW() failed: "
                   << GetLastSystemErrorString();
        return;
    }

    buffer_.resize(size);

    if (!GetLogicalDriveStringsW(size, buffer_.data()))
    {
        LOG(ERROR) << "GetLogicalDriveStringsW() failed: "
                   << GetLastSystemErrorString();
        return;
    }

    next_ = buffer_.data();
}

std::wstring DriveEnumerator::Next()
{
    if (!next_ || !next_[0])
        return std::wstring();

    current_.assign(next_);

    next_ = wcschr(next_, 0) + 1;

    return current_;
}

DriveEnumerator::DriveInfo DriveEnumerator::GetInfo() const
{
    return DriveInfo(current_);
}

DriveEnumerator::DriveInfo::DriveInfo(const std::wstring& path) :
    path_(path)
{
    // Nothing
}

std::wstring DriveEnumerator::DriveInfo::Path() const
{
    return path_;
}

DriveEnumerator::DriveInfo::DriveType DriveEnumerator::DriveInfo::Type() const
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
    }

    return DriveType::UNKNOWN;
}

uint64_t DriveEnumerator::DriveInfo::TotalSpace() const
{
    ULARGE_INTEGER total_space;

    if (!GetDiskFreeSpaceExW(path_.c_str(),
                             nullptr,
                             &total_space,
                             nullptr))
    {
        DLOG(ERROR) << "GetDiskFreeSpaceExW() failed: "
                    << GetLastSystemErrorString();
        return 0;
    }

    return total_space.QuadPart;
}

uint64_t DriveEnumerator::DriveInfo::FreeSpace() const
{
    ULARGE_INTEGER free_space;

    if (!GetDiskFreeSpaceExW(path_.c_str(),
                             nullptr,
                             nullptr,
                             &free_space))
    {
        DLOG(ERROR) << "GetDiskFreeSpaceExW() failed: "
                    << GetLastSystemErrorString();
        return 0;
    }

    return free_space.QuadPart;
}

std::wstring DriveEnumerator::DriveInfo::FileSystem() const
{
    wchar_t fs[MAX_PATH];

    if (!GetVolumeInformationW(path_.c_str(), nullptr,
                               0,
                               nullptr, nullptr, nullptr,
                               fs, _countof(fs)))
    {
        DLOG(ERROR) << "GetVolumeInformationW() failed: "
                    << GetLastSystemErrorString();
        return std::wstring();
    }

    return fs;
}

std::wstring DriveEnumerator::DriveInfo::VolumeName() const
{
    wchar_t name[MAX_PATH];

    if (!GetVolumeInformationW(path_.c_str(), name,
                               _countof(name),
                               nullptr, nullptr, nullptr,
                               nullptr, 0))
    {
        DLOG(ERROR) << "GetVolumeInformationW() failed: "
                    << GetLastSystemErrorString();
        return std::wstring();
    }

    return name;
}

} // namespace aspia
