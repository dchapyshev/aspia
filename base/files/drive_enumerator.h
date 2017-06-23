//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/files/drive_enumerator.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__FILES__DRIVE_ENUMERATOR_H
#define _ASPIA_BASE__FILES__DRIVE_ENUMERATOR_H

#include "base/macros.h"

#include <string>
#include <vector>

namespace aspia {

class DriveEnumerator
{
public:
    DriveEnumerator();
    ~DriveEnumerator() = default;

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

        std::wstring Path() const;
        DriveType Type() const;
        uint64_t TotalSpace() const;
        uint64_t FreeSpace() const;
        std::wstring FileSystem() const;
        std::wstring VolumeName() const;

    private:
        friend class DriveEnumerator;

        DriveInfo(const std::wstring& path);

        std::wstring path_;
    };

    std::wstring Next();

    DriveInfo GetInfo() const;

private:
    std::vector<wchar_t> buffer_;
    wchar_t* next_ = nullptr;
    std::wstring current_;

    DISALLOW_COPY_AND_ASSIGN(DriveEnumerator);
};

} // namespace aspia

#endif // _ASPIA_BASE__FILES__DRIVE_ENUMERATOR_H
