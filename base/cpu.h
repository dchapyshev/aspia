//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/cpu.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__CPU_H
#define _ASPIA_BASE__CPU_H

#include "aspia_config.h"

namespace aspia {

static INLINE int GetNumberOfProcessors()
{
    SYSTEM_INFO system_info = { 0 };
    GetNativeSystemInfo(&system_info);
    return system_info.dwNumberOfProcessors;
}

} // namespace aspia

#endif // _ASPIA_BASE__CPU_H
