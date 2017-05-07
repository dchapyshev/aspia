//
// PROJECT:         Aspia Remote Desktop
// FILE:            global.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_GLOBAL_H
#define _ASPIA_GLOBAL_H

//
// NOTE: This file is included to all *.cc files in the project over
// "Force include headers" in compiller flags.
// Do not include this file directly!
//

#include "build_config.h"

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

#define INLINE   __forceinline

#endif // _ASPIA_GLOBAL_H
