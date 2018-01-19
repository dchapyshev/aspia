//
// PROJECT:         Aspia
// FILE:            base/files/base_paths.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__FILES__BASE_PATHS_H
#define _ASPIA_BASE__FILES__BASE_PATHS_H

#include <experimental/filesystem>

namespace aspia {

enum class BasePathKey
{
    DIR_CURRENT,
    DIR_WINDOWS,         // Windows directory, usually "c:\windows"
    DIR_SYSTEM,          // Usually c:\windows\system32"

    DIR_COMMON_APP_DATA, // Usually "C:\ProgramData".
    DIR_COMMON_DESKTOP,  // Directory for the common desktop (visible on all user's Desktop).

    DIR_APP_DATA,        // Application Data directory under the user profile.

    DIR_USER_DESKTOP,    // The current user's Desktop.
    DIR_USER_HOME,       // The current user's Home.

    FILE_EXE
};

bool GetBasePath(BasePathKey key, std::experimental::filesystem::path& result);

} // namespace aspia

#endif // _ASPIA_BASE__FILES__BASE_PATHS_H
