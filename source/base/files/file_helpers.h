//
// PROJECT:         Aspia
// FILE:            base/files/file_helpers.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__FILES__FILE_HELPERS_H
#define _ASPIA_BASE__FILES__FILE_HELPERS_H

#include <experimental/filesystem>

namespace aspia {

bool IsValidFileName(const std::experimental::filesystem::path& path);

bool IsValidPathName(const std::experimental::filesystem::path& path);

} // namespace aspia

#endif // _ASPIA_BASE__FILES__FILE_HELPERS_H
