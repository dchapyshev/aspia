//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/files/file_helpers.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__FILES__FILE_HELPERS_H
#define _ASPIA_BASE__FILES__FILE_HELPERS_H

#include "base/files/file_path.h"

namespace aspia {

bool IsValidFileName(const FilePath& path);

bool IsValidPathName(const FilePath& path);

} // namespace aspia

#endif // _ASPIA_BASE__FILES__FILE_HELPERS_H
