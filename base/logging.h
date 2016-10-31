/*
* PROJECT:         Aspia Remote Desktop
* FILE:            base/logging.h
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#ifndef _ASPIA_BASE_LOGGING_H
#define _ASPIA_BASE_LOGGING_H

#undef ERROR

#define GLOG_NO_ABBREVIATED_SEVERITIES
#define GOOGLE_GLOG_DLL_DECL
#include "3rdparty/libglog/src/windows/glog/logging.h"

#endif // _ASPIA_BASE_LOGGING_H
