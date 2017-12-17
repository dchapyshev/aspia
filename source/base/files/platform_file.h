//
// PROJECT:         Aspia
// FILE:            base/files/platform_file.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__FILES__PLATFORM_FILE_H
#define _ASPIA_BASE__FILES__PLATFORM_FILE_H

#include "base/scoped_object.h"

// This file defines platform-independent types for dealing with
// platform-dependent files. If possible, use the higher-level base::File class
// rather than these primitives.

namespace aspia {

using PlatformFile = HANDLE;
using ScopedPlatformFile = ScopedHandle;

// It would be nice to make this constexpr but INVALID_HANDLE_VALUE is a
// ((void*)(-1)) which Clang rejects since reinterpret_cast is technically
// disallowed in constexpr. Visual Studio accepts this, however.
const PlatformFile kInvalidPlatformFile = INVALID_HANDLE_VALUE;

} // namespace aspia

#endif // _ASPIA_BASE__FILES__PLATFORM_FILE_H
