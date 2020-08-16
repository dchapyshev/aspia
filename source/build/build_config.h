//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef ASPIA_BUILD_CONFIG_H
#define ASPIA_BUILD_CONFIG_H

// OS detection.
#if defined(_WIN32)
#define OS_WIN
#elif defined(linux)
#define OS_LINUX
#else
#error Unknown OS
#endif

// Compiler detection.
#if defined(_MSC_VER)
#define CC_MSVC
#elif defined(__GNUC__)
#define CC_GCC
#else
#error Unknown compiller
#endif

// Architecture detection.
#if defined(_M_X64) || defined(__x86_64__)
#define ARCH_CPU_X86_FAMILY    1
#define ARCH_CPU_X86_64        1
#define ARCH_CPU_64_BITS       1
#define ARCH_CPU_LITTLE_ENDIAN 1
#elif defined(_M_IX86) || defined(__i386__)
#define ARCH_CPU_X86_FAMILY    1
#define ARCH_CPU_X86           1
#define ARCH_CPU_32_BITS       1
#define ARCH_CPU_LITTLE_ENDIAN 1
#else
#error Unknown architecture
#endif

#if defined(CC_MSVC)
#define FORCEINLINE __forceinline
#elif defined(CC_GCC) && __GNUC__ > 3
#define FORCEINLINE inline __attribute__ ((__always_inline__))
#else
#define FORCEINLINE inline
#endif

#if defined(OS_WIN)
#define WCHAR_T_IS_UTF16
#endif

#define DEFAULT_LOCALE        "en"
#define DEFAULT_UPDATE_SERVER u"https://update.aspia.org"

#define DEFAULT_HOST_TCP_PORT             8050
#define DEFAULT_ROUTER_TCP_PORT           8060
#define DEFAULT_PROXY_PEER_TCP_PORT       8070

#define ENABLE_LOCATION_SOURCE

#endif // ASPIA_BUILD_CONFIG_H
