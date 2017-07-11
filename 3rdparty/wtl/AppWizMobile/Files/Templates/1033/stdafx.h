// stdafx.h : include file for standard system include files,
//  or project specific include files that are used frequently, but
//      are changed infrequently
//

#pragma once

// Change this value to use different versions
#define WINVER 0x0420
[!if WTL_ENABLE_AX]
#define _CE_ALLOW_SINGLE_THREADED_OBJECTS_IN_MTA
[!endif]
#include <atlbase.h>
[!if WTL_USE_STRING]
[!if WTL_STRING_ATL]
#include <atlstr.h>

#define _WTL_NO_CSTRING
[!else]

#define _WTL_USE_CSTRING
[!endif]
[!else]

#define _WTL_NO_CSTRING
[!endif]
#include <atlapp.h>

extern CAppModule _Module;

[!if WTL_ENABLE_AX]
[!if WTL_EVC_COMPAT]
#include <atlwin.h>
#include <atlcom.h>
[!endif]
#include <atlhost.h>
#include <atlctl.h>
[!else]
#include <atlwin.h>
[!endif]

[!if WTL_EVC_COMPAT]
#if _WIN32_WCE >= 420
#include <tpcshell.h>
#endif
[!else]
#include <tpcshell.h>
[!endif]
#include <aygshell.h>
#pragma comment(lib, "aygshell.lib")
[!if WTL_USE_CPP_FILES]

#include <atlframe.h>
#include <atlctrls.h>
[!if WTL_USE_VIEW && WTL_VIEWTYPE_PROPSHEET]
#define _WTL_NEW_PAGE_NOTIFY_HANDLERS
#include <atldlgs.h>
[!endif]
[!if WTL_USE_STRING && !WTL_STRING_ATL]
#include <atlmisc.h>
[!endif]
[!if WTL_VIEW_SCROLL || WTL_VIEW_ZOOM]
[!if !WTL_USE_STRING || WTL_STRING_ATL]
#include <atlmisc.h>
[!endif]
#include <atlscrl.h>
[!else]
#define _WTL_CE_NO_ZOOMSCROLL
[!endif]
[!if !WTL_FULLSCREEN]
#define _WTL_CE_NO_FULLSCREEN
[!endif]
#include <atlwince.h>
[!endif]
