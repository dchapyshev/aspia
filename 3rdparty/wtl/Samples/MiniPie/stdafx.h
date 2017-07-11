// stdafx.h : include file for standard system include files,
//  or project specific include files that are used frequently, but
//      are changed infrequently
//

#pragma once

// Change this value to use different versions
#define WINVER 0x0420
#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS  // some CString constructors will be explicit
#define _CE_ALLOW_SINGLE_THREADED_OBJECTS_IN_MTA
#include <atlbase.h>
#include <atlstr.h>

#define _WTL_NO_CSTRING
#include <atlapp.h>

extern CAppModule _Module;

#include <atlhost.h>
#include <atlctl.h>

#include <pvdispid.h>
#include <piedocvw.h>

#include <tpcshell.h>
#include <aygshell.h>
#pragma comment(lib, "aygshell.lib")

#include <atlframe.h>
#include <atlmisc.h>
#include <atlctrls.h>
#include <atlddx.h>
#define _WTL_CE_NO_ZOOMSCROLL
#include <atlwince.h>
