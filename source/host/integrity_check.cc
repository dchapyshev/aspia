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
#include "base/files/base_paths.h"
#include "base/strings/unicode.h"

namespace host {

//--------------------------------------------------------------------------------------------------
bool integrityCheck()
{
#if defined(OS_WIN)
    static const char16_t* kFiles[] =
    {
        u"aspia_host.exe",
        u"aspia_desktop_agent.exe",
        u"aspia_file_transfer_agent.exe",
        u"aspia_host_service.exe"
    };
    static const size_t kMinFileSize = 5 * 1024; // 5 kB.

    std::filesystem::path current_dir;
    if (!base::BasePaths::currentExecDir(&current_dir))
    {
        LOG(LS_ERROR) << "Failed to get the directory of the current executable file";
        return false;
    }

    std::filesystem::path current_file;
    if (!base::BasePaths::currentExecFile(&current_file))
    {
        LOG(LS_ERROR) << "Failed to get the path to the current executable file";
        return false;
    }

    bool current_file_found = false;

    for (size_t i = 0; i < std::size(kFiles); ++i)
    {
        std::filesystem::path file_path(current_dir);
        file_path.append(kFiles[i]);

        if (file_path == current_file)
            current_file_found = true;

        std::error_code error_code;
        std::filesystem::file_status status = std::filesystem::status(file_path, error_code);
        if (error_code)
        {
            LOG(LS_ERROR) << "Failed to get file status '" << file_path << "': " << error_code;
            return false;
        }

        if (!std::filesystem::exists(status))
        {
            LOG(LS_ERROR) << "File '" << file_path << "' does not exist";
            return false;
        }

        if (!std::filesystem::is_regular_file(status))
        {
            LOG(LS_ERROR) << "File '" << file_path << "' is not a file";
            return false;
        }

        uintmax_t file_size = std::filesystem::file_size(file_path, error_code);
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
#endif // defined(OS_WIN)

    return true;
}

} // namespace host
