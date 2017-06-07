//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/path.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__PATH_H
#define _ASPIA_BASE__PATH_H

#include <string>

namespace aspia {

enum class PathKey
{
    DIR_CURRENT,
    DIR_WINDOWS,         // Windows directory, usually "c:\windows"
    DIR_SYSTEM,          // Usually c:\windows\system32"

    DIR_COMMON_APP_DATA, // Usually "C:\ProgramData".
    DIR_COMMON_DESKTOP,  // Directory for the common desktop (visible
                         // on all user's Desktop).

    DIR_USER_DESKTOP,    // The current user's Desktop.

    FILE_EXE
};

bool GetPathW(PathKey key, std::wstring& result);
bool GetPath(PathKey key, std::string& result);

} // namespace aspia

#endif // _ASPIA_BASE__PATH_H
