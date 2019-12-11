//
// Aspia Project
// Copyright (C) 2019 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "build/build_config.h"

#if defined(OS_WIN)
#include <Windows.h>
#endif

namespace base {

void debugPrint(const char* str)
{
#if defined(OS_WIN)
    OutputDebugStringA(str);
#else
#warning Platform support not implemented
#endif
}

void debugBreak()
{
#if defined(ARCH_CPU_X86_FAMILY)

#if defined(CC_MSVC)
    __debugbreak();
#elif defined(CC_GCC)
    __asm__("int3");
#elif defined(CC_CLANG)

#if defined(OS_WIN)
    { __asm int 3 };
#else
    __asm__("int3");
#endif // OS_WIN

#else // CC_*
#define Compiller support not implemented
#endif // CC_*

#else // ARCH_CPU_*
#error CPU family support not implemented
#endif // ARCH_CPU_*
}

} // namespace base
