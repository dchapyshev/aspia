// [!output PROJECT_NAME].cpp : main source file for [!output PROJECT_NAME].exe
//

#include "stdafx.h"
[!if !WTL_USE_CPP_FILES]

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

//?ppc
#include "resourceppc.h"
//?sp
#include "resourcesp.h"
//?end

[!if WTL_APPTYPE_SDI && WTL_USE_VIEW_CLASS]
#include "[!output WTL_VIEW_FILE].h"
[!endif]
#include "AboutDlg.h"
#include "[!output WTL_APPWND_FILE].h"

CAppModule _Module;

int WINAPI _tWinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPTSTR lpstrCmdLine, int nCmdShow)
{
	HRESULT hRes = [!output WTL_APPWND_CLASS]::ActivatePreviousInstance(hInstance, lpstrCmdLine);

	if(FAILED(hRes) || S_FALSE == hRes)
	{
		return hRes;
	}

	hRes = ::CoInitializeEx(NULL, COINIT_MULTITHREADED);
	ATLASSERT(SUCCEEDED(hRes));

	AtlInitCommonControls(ICC_DATE_CLASSES);
	SHInitExtraControls();
[!if WTL_VIEWTYPE_HTML]
	InitHTMLControl(hInstance);
[!endif]
[!if WTL_VIEWTYPE_INKX]
	InitInkX();
[!endif]
[!if WTL_VIEWTYPE_RICHINK]
	InitRichInkDLL();
[!endif]

	hRes = _Module.Init(NULL, hInstance);
	ATLASSERT(SUCCEEDED(hRes));

[!if WTL_ENABLE_AX]
	AtlAxWinInit();

[!endif]
	int nRet = [!output WTL_APPWND_CLASS]::AppRun(lpstrCmdLine, nCmdShow);

[!if WTL_ENABLE_AX]
	AtlAxWinTerm();

[!endif]
	_Module.Term();
	::CoUninitialize();

	return nRet;
}

