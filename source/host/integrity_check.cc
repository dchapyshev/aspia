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

#include "host/integrity_check.h"

#include "base/logging.h"

#include <QCoreApplication>
#include <QFileInfo>

namespace host {

//--------------------------------------------------------------------------------------------------
bool integrityCheck()
{
#if defined(Q_OS_WINDOWS)
    static const char* kFiles[] =
    {
        "aspia_host.exe",
        "aspia_desktop_agent.exe",
        "aspia_file_transfer_agent.exe",
        "aspia_host_service.exe"
    };
    static const size_t kMinFileSize = 5 * 1024; // 5 kB.

    QString current_dir = QCoreApplication::applicationDirPath();
    QString current_file = QCoreApplication::applicationFilePath();

    bool current_file_found = false;

    for (size_t i = 0; i < std::size(kFiles); ++i)
    {
        QString file_path(current_dir);

        file_path.append('/');
        file_path.append(kFiles[i]);

        if (file_path == current_file)
            current_file_found = true;

        QFileInfo file_info(file_path);

        if (!file_info.exists(file_path))
        {
            LOG(LS_ERROR) << "File '" << file_path << "' does not exist";
            return false;
        }

        if (!file_info.isFile() || file_info.isShortcut() || file_info.isSymLink())
        {
            LOG(LS_ERROR) << "File '" << file_path << "' is not a file";
            return false;
        }

        qint64 file_size = file_info.size();
        if (file_size < kMinFileSize)
        {
            LOG(LS_ERROR) << "File '" << file_path << "' is not the correct size: " << file_size;
            return false;
        }
    }

    if (!current_file_found)
    {
        LOG(LS_ERROR) << "Current executable file was not found in the list of components";
        return false;
    }
#endif // defined(Q_OS_WINDOWS)

    return true;
}

} // namespace host
