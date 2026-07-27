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

#include "base/physical_drive_reader.h"

#include "base/logging.h"

#if defined(Q_OS_WINDOWS)
#include "base/physical_drive_reader_win.h"
#elif defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
#include "base/physical_drive_reader_linux.h"
#endif

//--------------------------------------------------------------------------------------------------
// static
QStringList PhysicalDriveReader::devicePaths()
{
#if defined(Q_OS_WINDOWS)
    return PhysicalDriveReaderWin::devicePaths();
#elif defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    return PhysicalDriveReaderLinux::devicePaths();
#else
    NOTIMPLEMENTED();
    return QStringList();
#endif
}

//--------------------------------------------------------------------------------------------------
// static
std::unique_ptr<PhysicalDriveReader> PhysicalDriveReader::create(const QString& device_path)
{
    std::unique_ptr<PhysicalDriveReader> reader;

#if defined(Q_OS_WINDOWS)
    reader = std::make_unique<PhysicalDriveReaderWin>();
#elif defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    reader = std::make_unique<PhysicalDriveReaderLinux>();
#endif

    if (!reader)
    {
        NOTIMPLEMENTED();
        return nullptr;
    }

    if (!reader->open(device_path))
        return nullptr;

    return reader;
}
