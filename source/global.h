//
// PROJECT:         Aspia
// FILE:            global.h
// LICENSE:         GNU Lesser General Public License 2.1
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

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// Remove the min and max definitions. We use std::min and std::max.
#ifdef min
#undef min
#endif // min

#ifdef max
#undef max
#endif // max

#endif // _ASPIA_GLOBAL_H
