// BmpView.cpp : main source file for BmpView.exe
//

#include "stdafx.h"

#include <atlframe.h>
#include <atlctrls.h>
#ifndef _WIN32_WCE
#include <atlctrlw.h>
#endif // _WIN32_WCE
#include <atldlgs.h>
#include <atlscrl.h>
#include <atlmisc.h>
#ifndef _WIN32_WCE
#include <atlprint.h>
#endif // _WIN32_WCE
#include <atlcrack.h>

#ifndef _WIN32_WCE
#include "resource.h"
#else // _WIN32_WCE
#ifndef WIN32_PLATFORM_PSPC
#include "resourcece.h"
#else // WIN32_PLATFORM_PSPC
#include "resourceppc.h"
#endif // WIN32_PLATFORM_PSPC
#endif // _WIN32_WCE

#include "View.h"
#include "props.h"
#ifndef _WIN32_WCE
#include "list.h"
#endif // _WIN32_WCE
#include "MainFrm.h"

CAppModule _Module;


int Run(LPTSTR /*lpstrCmdLine*/ = NULL, int nCmdShow = 
#ifndef _WIN32_WCE
		SW_SHOWDEFAULT
#else // _WIN32_WCE
		SW_SHOWNORMAL
#endif // _WIN32_WCE
		)
{
	CMessageLoop theLoop;
	_Module.AddMessageLoop(&theLoop);

	CMainFrame wndMain;

	if(wndMain.CreateEx() == NULL)
	{
		ATLTRACE(_T("Main window creation failed!\n"));
		return 0;
	}

	wndMain.ShowWindow(nCmdShow);

	int nRet = theLoop.Run();

	_Module.RemoveMessageLoop();
	return nRet;
}

int WINAPI _tWinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPTSTR lpstrCmdLine, int nCmdShow)
{
#ifndef _WIN32_WCE
	INITCOMMONCONTROLSEX iccx;
	iccx.dwSize = sizeof(iccx);
	iccx.dwICC = ICC_COOL_CLASSES | ICC_BAR_CLASSES;
	BOOL bRet = ::InitCommonControlsEx(&iccx);
	bRet;
	ATLASSERT(bRet);
#endif // _WIN32_WCE

	HRESULT hRes = _Module.Init(NULL, hInstance);
	hRes;
	ATLASSERT(SUCCEEDED(hRes));

	int nRet = Run(lpstrCmdLine, nCmdShow);

	_Module.Term();
	return nRet;
}
