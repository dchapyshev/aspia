//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/path.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__PATH_H
#define _ASPIA_BASE__PATH_H

#include "aspia_config.h"
#include <string>

namespace aspia {

enum class PathKey
{
    DIR_CURRENT,
    DIR_WINDOWS, // Windows directory, usually "c:\windows"
    DIR_SYSTEM,  // Usually c:\windows\system32"

    FILE_EXE
};

bool GetPath(PathKey key, std::wstring* result);
bool GetPath(PathKey key, std::string* result);

} // namespace aspia

#endif // _ASPIA_BASE__PATH_H
