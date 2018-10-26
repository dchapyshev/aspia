//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "build/build_config.h"

#ifdef OS_LINUX

#include "base/base_paths.h"
#include "base/logging.h"
#include <filesystem>
#include <cstdio>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <climits>

namespace aspia {

// static
bool BasePaths::windowsDir(std::filesystem::path* result)
{
    PLOG(LS_ERROR) << "Not supported on Linux";
    return false;
}

// static
bool BasePaths::systemDir(std::filesystem::path* result)
{
    PLOG(LS_WARNING) << "Not supported on Linux";
    return false;
}

// static
bool BasePaths::userAppData(std::filesystem::path* result)
{
    DCHECK(result);

    result->assign("~/.cache");
    return true;
}

// static
bool BasePaths::userDesktop(std::filesystem::path* result)
{
    DCHECK(result);

    result->assign("~/Desktop");
    return true;
}

// static
bool BasePaths::userHome(std::filesystem::path* result)
{
    DCHECK(result);

    result->assign("~");
    return true;
}

// static
bool BasePaths::commonAppData(std::filesystem::path* result)
{
    DCHECK(result);

    result->assign("/var/lib");
    return true;
}

// static
bool BasePaths::commonDesktop(std::filesystem::path* result)
{
    PLOG(LS_WARNING) << "Not supported on Linux";
    return false;
}

// static
bool BasePaths::currentExecDir(std::filesystem::path* result)
{
    DCHECK(result);

    std::filesystem::path exe_path;

    if (!currentExecFile(&exe_path))
        return false;

    *result = exe_path.parent_path();
    return true;
}

// static
bool BasePaths::currentExecFile(std::filesystem::path* result)
{
    DCHECK(result);

    pid_t pid = getpid();
    char procPath[PATH_MAX];
    sprintf(procPath, "/proc/%d/exe", pid);
    char execPath[PATH_MAX];
    if (readlink(procPath, execPath, PATH_MAX - 1) == -1)
        perror("readlink");
    else {
        printf("%s\n", execPath);
    }

    result->assign(execPath);
    return true;
}

} // namespace aspia

#endif //OS_LINUX