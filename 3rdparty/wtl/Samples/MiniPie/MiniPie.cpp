// MiniPie.cpp : main source file for MiniPie.exe
//

#include "stdafx.h"

#ifdef WIN32_PLATFORM_PSPC
#include "resourceppc.h"
#else
#include "resourcesp.h"
#endif

#include "MiniPieFrame.h"

CAppModule _Module;

int WINAPI _tWinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPTSTR lpstrCmdLine, int nCmdShow)
{
	HRESULT hRes = CMiniPieFrame::ActivatePreviousInstance(hInstance, lpstrCmdLine);

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

	AtlAxWinInit();

	int nRet = CMiniPieFrame::AppRun(lpstrCmdLine, nCmdShow);

	AtlAxWinTerm();

	_Module.Term();
	::CoUninitialize();

	return nRet;
}

