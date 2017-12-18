#include "stdafx.h"

#include <atlframe.h>
#include <atlgdi.h>
#include <atldlgs.h>

#include "resource.h"
#include "mainfrm.h"

CAppModule _Module;

int Run(LPSTR lpCmdLine = NULL, int nCmdShow = SW_SHOWDEFAULT)
{
	CMessageLoop theLoop;
	_Module.AddMessageLoop(&theLoop);

	CMDIFrame wndMain;

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

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPSTR lpCmdLine, int nCmdShow)
{
	::InitCommonControls();
	_Module.Init(NULL, hInstance);

	int nRet = Run(lpCmdLine, nCmdShow);

	_Module.Term();
	return nRet;
}
