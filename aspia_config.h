/*
* PROJECT:         Aspia Remote Desktop
* FILE:            aspia_config.h
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#ifndef _ASPIA_CONFIG_H
#define _ASPIA_CONFIG_H

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

//
// Target version
// ћы используем Windows XP в качестве минимальной версии
//
#define WINVER           0x0501
#define _WIN32_WINNT     0x0501
#define _WIN32_WINDOWS   0x0501
#define NTDDI_VERSION    0x05010000
#define _WIN32_IE        0x0600
#define PSAPI_VERSION    1

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#ifdef ERROR
#undef ERROR
#endif // ERROR

#ifdef min
#undef min
#endif // min

#ifdef max
#undef max
#endif // max

#endif // _ASPIA_CONFIG_H
