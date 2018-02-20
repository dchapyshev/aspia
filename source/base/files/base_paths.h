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
    DIR_WINDOWS,           // Windows directory, usually "c:\windows"
    DIR_SYSTEM,            // Usually c:\windows\system32"

    DIR_APP_DATA,          // Application Data directory under the user profile.

    DIR_USER_DESKTOP,      // The current user's Desktop.
    DIR_USER_HOME,         // The current user's Home.

    DIR_EXE,
    FILE_EXE
};

bool GetBasePath(BasePathKey key, std::experimental::filesystem::path& result);

} // namespace aspia

#endif // _ASPIA_BASE__FILES__BASE_PATHS_H
