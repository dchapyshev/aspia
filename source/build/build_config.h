//
// Aspia Project
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

#ifndef ASPIA_BUILD_CONFIG_H
#define ASPIA_BUILD_CONFIG_H

// OS detection.
#if defined(_WIN32)
#define OS_WIN 1
#elif defined(linux) || defined(__linux__)
#define OS_LINUX 1
#elif defined(__APPLE__)
#define OS_MAC 1
#define OS_MACOS 1
#define OS_MACOSX 1
#else
#error Unknown OS
#endif

#if defined(OS_LINUX) || defined(OS_MAC)
#define OS_POSIX 1
#define OS_UNIX 1
#endif

// Compiler detection. Note: clang masquerades as GCC on POSIX and as MSVC on Windows.
#if defined(_MSC_VER)
#define CC_MSVC 1
#elif defined(__GNUC__)
#define CC_GCC 1
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

#elif defined(__ARMEL__)

#define ARCH_CPU_ARM_FAMILY    1
#define ARCH_CPU_ARMEL         1
#define ARCH_CPU_32_BITS       1
#define ARCH_CPU_LITTLE_ENDIAN 1

#elif defined(__aarch64__) || defined(_M_ARM64)

#define ARCH_CPU_ARM_FAMILY    1
#define ARCH_CPU_ARM64         1
#define ARCH_CPU_64_BITS       1
#define ARCH_CPU_LITTLE_ENDIAN 1

#else
#error Unknown architecture
#endif

#if defined(OS_WIN)
#define WCHAR_T_IS_UTF16
#elif defined(OS_POSIX) && defined(CC_GCC) && defined(__WCHAR_MAX__) && \
    (__WCHAR_MAX__ == 0x7fffffff || __WCHAR_MAX__ == 0xffffffff)
#define WCHAR_T_IS_UTF32
#elif defined(OS_POSIX) && defined(CC_GCC) && defined(__WCHAR_MAX__) && \
    (__WCHAR_MAX__ == 0x7fff || __WCHAR_MAX__ == 0xffff)
#define WCHAR_T_IS_UTF16
#else
#error Unknown wchar_t size
#endif

#define DEFAULT_LOCALE        "en"
#define DEFAULT_UPDATE_SERVER "https://update.aspia.net"

#define DEFAULT_HOST_TCP_PORT             8050
#define DEFAULT_ROUTER_TCP_PORT           8060
#define DEFAULT_RELAY_PEER_TCP_PORT       8070

#define ENABLE_LOCATION_SOURCE

#endif // ASPIA_BUILD_CONFIG_H
