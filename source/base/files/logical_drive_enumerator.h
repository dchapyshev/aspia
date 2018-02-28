//
// PROJECT:         Aspia
// FILE:            base/files/logical_drive_enumerator.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__FILES__LOGICAL_DRIVE_ENUMERATOR_H
#define _ASPIA_BASE__FILES__LOGICAL_DRIVE_ENUMERATOR_H

#include "base/macros.h"

#include <experimental/filesystem>
#include <vector>

namespace aspia {

class LogicalDriveEnumerator
{
public:
    LogicalDriveEnumerator();
    ~LogicalDriveEnumerator() = default;

    class DriveInfo
    {
    public:
        ~DriveInfo() = default;

        enum class DriveType
        {
            UNKNOWN,
            REMOVABLE, // Floppy, flash drives.
            FIXED,     // Hard or flash drives.
            REMOTE,    // Network drives.
            CDROM,     // CD, DVD drives.
            RAM        // RAM drives.
        };

        std::experimental::filesystem::path Path() const;
        DriveType Type() const;
        uint64_t TotalSpace() const;
        uint64_t FreeSpace() const;
        std::wstring FileSystem() const;
        std::wstring VolumeName() const;
        std::string VolumeSerial() const;

    private:
        friend class LogicalDriveEnumerator;

        explicit DriveInfo(const std::experimental::filesystem::path& path);

        std::experimental::filesystem::path path_;
    };

    std::experimental::filesystem::path Next();

    DriveInfo GetInfo() const;

private:
    std::vector<wchar_t> buffer_;
    wchar_t* next_ = nullptr;
    std::wstring current_;

    DISALLOW_COPY_AND_ASSIGN(LogicalDriveEnumerator);
};

} // namespace aspia

#endif // _ASPIA_BASE__FILES__LOGICAL_DRIVE_ENUMERATOR_H
