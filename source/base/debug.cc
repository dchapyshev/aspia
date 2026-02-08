//
// SmartCafe Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#include "base/debug.h"

#include <QtGlobal>

#if defined(Q_OS_WINDOWS)
#include <qt_windows.h>
#endif

namespace base {

//--------------------------------------------------------------------------------------------------
bool isDebuggerPresent()
{
#if defined(Q_OS_WINDOWS)
    return !!IsDebuggerPresent();
#else
#warning Platform support not implemented
    return false;
#endif
}

//--------------------------------------------------------------------------------------------------
void debugPrint(const char* str)
{
#if defined(Q_OS_WINDOWS)
    OutputDebugStringA(str);
#else
#warning Platform support not implemented
#endif
}

//--------------------------------------------------------------------------------------------------
void debugBreak()
{
#if defined(Q_PROCESSOR_X86)

#if defined(Q_CC_MSVC)
    __debugbreak();
#elif defined(Q_CC_GNU)
    __asm__("int3");
#elif defined(Q_CC_CLANG)

#if defined(Q_OS_WINDOWS)
    { __asm int 3 };
#else
    __asm__("int3");
#endif // Q_OS_WINDOWS

#else // Q_CC_*
#define Compiller support not implemented
#endif // Q_CC_*

#elif defined(Q_PROCESSOR_ARM_64)
    asm("brk 0");
#elif defined(Q_PROCESSOR_ARM_32)
    asm("break 2");
#else // Q_PROCESSOR_*
#error CPU family support not implemented
#endif // Q_PROCESSOR_*
}

} // namespace base
