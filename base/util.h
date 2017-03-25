//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/util.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__UTIL_H
#define _ASPIA_BASE__UTIL_H

#include "aspia_config.h"
#include <string>

namespace aspia {

std::string ReplaceLfByCrLf(const std::string& in);

std::string ReplaceCrLfByLf(const std::string& in);

std::string StringPrintf(const char* format, ...);

std::wstring StringPrintfW(const WCHAR* format, ...);

} // namespace aspia

#endif // _ASPIA_BASE__UTIL_H
