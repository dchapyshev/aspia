// SPcontrols.cpp : main source file for SPcontrols.exe
//

#include "stdafx.h"

#include <atlframe.h>
#include <atlctrls.h>
#include <atldlgs.h>

#include <atlmisc.h>

#define _WTL_CE_NO_ZOOMSCROLL
#include <atlwince.h>

#include "resource.h"
#include "maindlg.h"

CAppModule _Module;

int WINAPI _tWinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPTSTR lpstrCmdLine, int nCmdShow)
{

	HRESULT hRes = CMainDlg::ActivatePreviousInstance(hInstance, lpstrCmdLine);
	
	if(FAILED(hRes) || S_FALSE == hRes)
	{
		return hRes;
	}

	hRes = ::CoInitializeEx(NULL, COINIT_MULTITHREADED);
	ATLASSERT(SUCCEEDED(hRes));

	AtlInitCommonControls(ICC_UPDOWN_CLASS);
	SHInitExtraControls();

	hRes = _Module.Init(NULL, hInstance);
	ATLASSERT(SUCCEEDED(hRes));

	int nRet = CMainDlg::AppRun(lpstrCmdLine, nCmdShow);

	_Module.Term();
	::CoUninitialize();

	return nRet;
}

