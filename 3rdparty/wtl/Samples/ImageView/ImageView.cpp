// ImageView.cpp : main source file for ImageView.exe
//

#include "stdafx.h"

#if (_WTL_VER < 0x0750) 
	#error ImageView requires WTL version 7.5 or higher
#endif

#include <atlframe.h>
#include <atlctrls.h>
#include <atldlgs.h>

#define _ATL_USE_CSTRING_FLOAT
#include <atlmisc.h>
#include <atlscrl.h>

#include <atlwince.h>

#include "resource.h"

#include "ImageViewView.h"
#include "ImageViewdlg.h"
#include "MainFrm.h"

CAppModule _Module;

int WINAPI _tWinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPTSTR lpstrCmdLine, int nCmdShow)
{

	HRESULT hRes = CMainFrame::ActivatePreviousInstance(hInstance, lpstrCmdLine );
	
	if(FAILED(hRes) || S_FALSE == hRes)
	{
		return hRes;
	}

	hRes = ::CoInitializeEx(NULL, COINIT_MULTITHREADED);
	ATLASSERT(SUCCEEDED(hRes));

	AtlInitCommonControls(ICC_DATE_CLASSES);
	SHInitExtraControls();

	hRes = _Module.Init(NULL, hInstance);
	ATLASSERT(SUCCEEDED(hRes));

	int nRet = CMainFrame::AppRun(lpstrCmdLine, nCmdShow);

	_Module.Term();
	::CoUninitialize();

	return nRet;
}
