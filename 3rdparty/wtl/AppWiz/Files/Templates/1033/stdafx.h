// stdafx.h : include file for standard system include files,
//  or project specific include files that are used frequently, but
//  are changed infrequently
//

#pragma once

// Change these values to use different versions
[!if WTL_USE_RIBBON]
#define WINVER		0x0601
#define _WIN32_WINNT	0x0601
#define _WIN32_IE	0x0700
[!else]
#define WINVER		0x0500
#define _WIN32_WINNT	0x0501
#define _WIN32_IE	0x0501
[!endif]
#define _RICHEDIT_VER	0x0500

[!if WTL_COM_SERVER]
#define _ATL_APARTMENT_THREADED

[!endif]
[!if WTL_USE_EXTERNAL_ATL]
// This project was generated for VC++ Express and external ATL from Platform SDK or DDK.
// Comment out this line to build the project with different versions of VC++ and ATL.
#define _WTL_SUPPORT_EXTERNAL_ATL

// Support for VC++ Express & external ATL
#ifdef _WTL_SUPPORT_EXTERNAL_ATL
  #define ATL_NO_LEAN_AND_MEAN
  #include <atldef.h>

  #if (_ATL_VER < 0x0800)
    #define _CRT_SECURE_NO_DEPRECATE
    #pragma conform(forScope, off)
    #pragma comment(linker, "/NODEFAULTLIB:atlthunk.lib")
  #endif
#endif // _WTL_SUPPORT_EXTERNAL_ATL

[!endif]
#include <atlbase.h>
[!if WTL_USE_EXTERNAL_ATL]

// Support for VC++ Express & external ATL
#ifdef _WTL_SUPPORT_EXTERNAL_ATL
  // for #pragma prefast
  #ifndef _PREFAST_
    #pragma warning(disable:4068)
  #endif

  #if (_ATL_VER >= 0x0800)
    // for _stdcallthunk
    #include <atlstdthunk.h>
    #pragma comment(lib, "atlthunk.lib")
  #else
    namespace ATL
    {
	inline void * __stdcall __AllocStdCallThunk()
	{
		return ::HeapAlloc(::GetProcessHeap(), 0, sizeof(_stdcallthunk));
	}

	inline void __stdcall __FreeStdCallThunk(void *p)
	{
		::HeapFree(::GetProcessHeap(), 0, p);
	}
    };
  #endif
#endif // _WTL_SUPPORT_EXTERNAL_ATL

[!endif]
#include <atlapp.h>

[!if WTL_COM_SERVER]
extern CServerAppModule _Module;

// This is here only to tell VC7 Class Wizard this is an ATL project
#ifdef ___VC7_CLWIZ_ONLY___
CComModule
CExeModule
#endif

[!else]
extern CAppModule _Module;

[!endif]
[!if WTL_ENABLE_AX || WTL_COM_SERVER]
#include <atlcom.h>
[!endif]
[!if WTL_ENABLE_AX]
#include <atlhost.h>
[!endif]
#include <atlwin.h>
[!if WTL_ENABLE_AX]
#include <atlctl.h>
[!endif]
[!if WTL_USE_CPP_FILES]

#include <atlframe.h>
#include <atlctrls.h>
#include <atldlgs.h>
[!if WTL_USE_CMDBAR]
#include <atlctrlw.h>
[!endif]
[!if WTL_APPTYPE_TABVIEW]
#include <atlctrlx.h>
[!endif]
[!if WTL_APPTYPE_EXPLORER]
#include <atlctrlx.h>
#include <atlsplit.h>
[!endif]
[!if WTL_USE_VIEW]
[!if WTL_VIEWTYPE_SCROLL]
#include <atlscrl.h>
[!endif]
[!endif]
[!if WTL_USE_RIBBON]
#include <atlribbon.h>
[!endif]
[!endif]
[!if WTL_USE_EMBEDDED_MANIFEST]

#if defined _M_IX86
  #pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_IA64
  #pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='ia64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_X64
  #pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#else
  #pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif
[!endif]
