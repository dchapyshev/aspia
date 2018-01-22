//
// PROJECT:         Aspia
// FILE:            build_config.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BUILD_CONFIG_H
#define _ASPIA_BUILD_CONFIG_H

#define OFFICIAL_BUILD 0

// Compiler detection.
#if defined(_MSC_VER)
#define COMPILER_MSVC 1
#else
#error Please add support for your compiler in build_config.h
#endif

// Processor architecture detection.
#if defined(_M_X64) || defined(__x86_64__)
#define ARCH_CPU_X86_FAMILY      1
#define ARCH_CPU_X86_64          1
#define ARCH_CPU_64_BITS         1
#define ARCH_CPU_LITTLE_ENDIAN   1
#elif defined(_M_IX86) || defined(__i386__)
#define ARCH_CPU_X86_FAMILY      1
#define ARCH_CPU_X86             1
#define ARCH_CPU_32_BITS         1
#define ARCH_CPU_LITTLE_ENDIAN   1
#else
#error Please add support for your processor architecture in build_config.h
#endif

// Target version
#if defined(ARCH_CPU_X86_64)
#define _WIN32_WINNT     0x0502
#define NTDDI_VERSION    0x05020000 // Windows 2003 / XP x64
#define _WIN32_IE        0x0800 // Internet Explorer 8.0
#define PSAPI_VERSION    1
#define WINVER           _WIN32_WINNT
#define _WIN32_WINDOWS   _WIN32_WINNT
#elif defined(ARCH_CPU_X86)
#define _WIN32_WINNT     0x0501
#define NTDDI_VERSION    0x05010300 // Windows XP SP3
#define _WIN32_IE        0x0800 // Internet Explorer 8.0
#define PSAPI_VERSION    1
#define WINVER           _WIN32_WINNT
#define _WIN32_WINDOWS   _WIN32_WINNT
#endif

static const unsigned short kDefaultHostTcpPort = 8050;

#endif // _ASPIA_BUILD_CONFIG_H
