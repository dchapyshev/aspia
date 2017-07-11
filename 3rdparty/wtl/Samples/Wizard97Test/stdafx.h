// stdafx.h : include file for standard system include files,
//  or project specific include files that are used frequently, but
//      are changed infrequently
//

#if !defined(AFX_STDAFX_H__215476E2_9EAF_445C_9A58_E8CBCF4461AE__INCLUDED_)
#define AFX_STDAFX_H__215476E2_9EAF_445C_9A58_E8CBCF4461AE__INCLUDED_

#pragma once

// Preprocessor definitions
#ifndef STRICT
#define STRICT
#endif

#define WINVER        0x0500
#define _WIN32_WINNT  0x0500  // Require Windows 2000 or later
#define _WIN32_IE     0x0501  // Require IE 5.01 or later (comes with Windows 2000 or later)
#define _RICHEDIT_VER 0x0300  // Require RichEdit 3.0 or later (comes with Windows 2000 or later)

// ATL related preprocessor definitions
#if (_ATL_VER >= 0x0700)
	#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS	// some CString constructors will be explicit
	#define _ATL_USE_CSTRING_FLOAT
	#define _ATL_USE_DDX_FLOAT
	#define _ATL_NO_OLD_NAMES
	#define _ATL_DISABLE_DEPRECATED
	#define _ATL_ALL_WARNINGS
	#define _ATL_NO_OLD_HEADERS_WIN64
#endif

// Includes
#include "resource.h"

#include <atlbase.h>
#if (_ATL_VER >= 0x0700)
	#include <atlstr.h>
	#include <atltypes.h>
#endif

// WTL related preprocessor definitions
#if (_ATL_VER >= 0x0700)
	#define _WTL_NO_WTYPES
	#define _WTL_NO_UNION_CLASSES
	#define _WTL_NO_CSTRING
#endif
#define _WTL_NEW_PAGE_NOTIFY_HANDLERS

#include <atlapp.h>
extern CAppModule _Module;

#include <atlwin.h>
#include <atlcom.h>

#include <atlmisc.h>
#include <atlctl.h>
#include <atlframe.h>
#include <atlddx.h>
#include <atldlgs.h>
#include <atlctrls.h>
#include <atlctrlw.h>
#include <atlctrlx.h>
#include <atlscrl.h>

#ifdef _EMBEDDED_MANIFEST
#if defined _M_IX86
  #pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_IA64
  #pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='ia64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_X64
  #pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#else
  #pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif
#endif // _EMBEDDED_MANIFEST

#endif // !defined(AFX_STDAFX_H__215476E2_9EAF_445C_9A58_E8CBCF4461AE__INCLUDED_)
