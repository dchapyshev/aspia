// stdafx.h : include file for standard system include files,
//  or project specific include files that are used frequently, but
//      are changed infrequently
//

// Change these values to use different versions
#define WINVER		0x0420

#define _WIN32_WCE_AYGSHELL 1

#include <atlbase.h>
#include <atlapp.h>

extern CAppModule _Module;

#if defined( DEBUG) && !defined( _DEBUG)
	#define _DEBUG
#endif

#include <atlwin.h>

#include <aygshell.h>
#pragma comment(lib, "aygshell.lib")

