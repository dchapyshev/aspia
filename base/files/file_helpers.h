//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/files/file_helpers.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__FILES__FILE_HELPERS_H
#define _ASPIA_BASE__FILES__FILE_HELPERS_H

#include <string>

namespace aspia {

bool IsValidFileName(const std::string& path);

bool IsValidPathName(const std::string& path);

} // namespace aspia

#endif // _ASPIA_BASE__FILES__FILE_HELPERS_H
