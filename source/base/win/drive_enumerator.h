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

#ifndef BASE_WIN_DRIVE_ENUMERATOR_H
#define BASE_WIN_DRIVE_ENUMERATOR_H

#include "base/macros_magic.h"

#include <filesystem>
#include <vector>

#include <QString>

namespace base {

class DriveEnumerator
{
public:
    DriveEnumerator();
    ~DriveEnumerator() = default;

    class DriveInfo
    {
    public:
        ~DriveInfo() = default;

        enum class Type
        {
            UNKNOWN,
            REMOVABLE, // Floppy, flash drives.
            FIXED,     // Hard or flash drives.
            REMOTE,    // Network drives.
            CDROM,     // CD, DVD drives.
            RAM        // RAM drives.
        };

        const std::filesystem::path& path() const { return path_; }
        Type type() const;
        quint64 totalSpace() const;
        quint64 freeSpace() const;
        QString fileSystem() const;
        QString volumeName() const;
        QString volumeSerial() const;

    private:
        friend class DriveEnumerator;
        DriveInfo() = default;

        std::filesystem::path path_;
    };

    const DriveInfo& driveInfo() const;
    bool isAtEnd() const;
    void advance();

private:
    std::vector<wchar_t> buffer_;
    wchar_t* current_ = nullptr;

    mutable DriveInfo drive_info_;

    DISALLOW_COPY_AND_ASSIGN(DriveEnumerator);
};

} // namespace base

#endif // BASE_WIN_DRIVE_ENUMERATOR_H
