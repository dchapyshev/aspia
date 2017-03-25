/*
* PROJECT:         Aspia Remote Desktop
* FILE:            aspia_config.h
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#ifndef _ASPIA_CONFIG_H
#define _ASPIA_CONFIG_H

#include <stdint.h>

static const uint16_t kDefaultHostTcpPort = 11011;

#ifdef WINVER
#undef WINVER
#endif // WINVER

#ifdef _WIN32_WINNT
#undef _WIN32_WINNT
#endif // _WIN32_WINNT

#ifdef _WIN32_WINDOWS
#undef _WIN32_WINDOWS
#endif // _WIN32_WINDOWS

#ifdef NTDDI_VERSION
#undef NTDDI_VERSION
#endif // NTDDI_VERSION

#ifdef _WIN32_IE
#undef _WIN32_IE
#endif // _WIN32_IE

#ifdef PSAPI_VERSION
#undef PSAPI_VERSION
#endif // PSAPI_VERSION

// Target version
#define _WIN32_WINNT     0x0601
#define NTDDI_VERSION    0x06010000 // Windows 7
#define _WIN32_IE        0x0800 // Internet Explorer 8.0
#define PSAPI_VERSION    2
#define WINVER           _WIN32_WINNT
#define _WIN32_WINDOWS   _WIN32_WINNT

// windows.h должен подключаться только тут и нигде больше.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// Убираем определение ERROR. Мешает использованию GLOG.
#ifdef ERROR
#undef ERROR
#endif // ERROR

// Убираем определения min и max. Мы используем std::min и std::max.
#ifdef min
#undef min
#endif // min

#ifdef max
#undef max
#endif // max

#define INLINE      __forceinline

#ifndef UNREFERENCED_PARAMETER
#define UNREFERENCED_PARAMETER(P)    (P)
#endif // UNREFERENCED_PARAMETER

#endif // _ASPIA_CONFIG_H
