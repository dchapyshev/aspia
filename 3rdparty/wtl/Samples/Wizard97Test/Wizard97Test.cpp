// Wizard97Test.cpp : main source file for Wizard97Test.exe
//

#include "stdafx.h"

#include "resource.h"

#include "Wizard\TestWizard.h"

CAppModule _Module;


int WINAPI _tWinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPTSTR /*lpstrCmdLine*/, int /*nCmdShow*/)
{
	HRESULT hRes = ::OleInitialize(NULL);
	ATLASSERT(SUCCEEDED(hRes));

	// this resolves ATL window thunking problem when Microsoft Layer for Unicode (MSLU) is used
	::DefWindowProc(NULL, 0, 0, 0L);

	AtlInitCommonControls(
		ICC_WIN95_CLASSES |
		ICC_DATE_CLASSES |
		ICC_USEREX_CLASSES |
		ICC_COOL_CLASSES |
		ICC_PAGESCROLLER_CLASS |
		ICC_NATIVEFNTCTL_CLASS);

	// We use a RichEdit control
	HINSTANCE hInstRich = ::LoadLibrary(CRichEditCtrl::GetLibraryName());
	ATLASSERT(hInstRich != NULL);

	hRes = _Module.Init(NULL, hInstance);
	ATLASSERT(SUCCEEDED(hRes));

	int nRet = 0;
	// BLOCK: Run application
	{
		CTestWizard wizard;
		wizard.ExecuteWizard();
	}

	::FreeLibrary(hInstRich);

	_Module.Term();
	::OleUninitialize();

	return nRet;
}
