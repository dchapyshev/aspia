/*
* PROJECT:         Aspia Remote Desktop
* FILE:            gui/stdafx.h
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

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
#include <shellapi.h>

#include <atlbase.h>
#include <atlapp.h>

#include <atlwin.h>
#include <atlgdi.h>
#include <atlctrls.h>
#include <atlsplit.h>
#include <atlctrlx.h>
#include <atlmisc.h>

#undef ERROR
#undef min
#undef max

#include <malloc.h>
#include <direct.h>
#include <intrin.h>

#include <mutex>
#include <vector>
#include <thread>
#include <utility>

#define GLOG_NO_ABBREVIATED_SEVERITIES
#define GOOGLE_GLOG_DLL_DECL
#include "glog\logging.h"
