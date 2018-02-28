//
// PROJECT:         Aspia
// FILE:            base/files/base_paths.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__FILES__BASE_PATHS_H
#define _ASPIA_BASE__FILES__BASE_PATHS_H

#include <experimental/filesystem>
#include <optional>

#include "base/macros.h"

namespace aspia {

class BasePaths
{
public:
    // Windows directory, usually "c:\windows"
    static std::optional<std::experimental::filesystem::path> GetWindowsDirectory();

    // Usually c:\windows\system32"
    static std::optional<std::experimental::filesystem::path> GetSystemDirectory();

    // Application Data directory under the user profile.
    static std::optional<std::experimental::filesystem::path> GetAppDataDirectory();

    // The current user's Desktop.
    static std::optional<std::experimental::filesystem::path> GetUserDesktopDirectory();

    // The current user's Home.
    static std::optional<std::experimental::filesystem::path> GetUserHomeDirectory();

    static std::optional<std::experimental::filesystem::path> GetCurrentExecutableDirectory();
    static std::optional<std::experimental::filesystem::path> GetCurrentExecutableFile();

private:
    DISALLOW_COPY_AND_ASSIGN(BasePaths);
};

} // namespace aspia

#endif // _ASPIA_BASE__FILES__BASE_PATHS_H
