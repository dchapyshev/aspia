//
// PROJECT:         Aspia
// FILE:            base/files/base_paths.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__FILES__BASE_PATHS_H
#define _ASPIA_BASE__FILES__BASE_PATHS_H

#include <experimental/filesystem>

#include "base/macros.h"

namespace aspia {

class BasePaths
{
public:
    // Windows directory, usually "c:\windows"
    static bool GetWindowsDirectory(std::experimental::filesystem::path& result);

    // Usually c:\windows\system32"
    static bool GetSystemDirectory(std::experimental::filesystem::path& result);

    // Application Data directory under the user profile.
    static bool GetAppDataDirectory(std::experimental::filesystem::path& result);

    // The current user's Desktop.
    static bool GetUserDesktopDirectory(std::experimental::filesystem::path& result);

    // The current user's Home.
    static bool GetUserHomeDirectory(std::experimental::filesystem::path& result);

    static bool GetCurrentExecutableDirectory(std::experimental::filesystem::path& result);
    static bool GetCurrentExecutableFile(std::experimental::filesystem::path& result);

private:
    DISALLOW_COPY_AND_ASSIGN(BasePaths);
};

} // namespace aspia

#endif // _ASPIA_BASE__FILES__BASE_PATHS_H
