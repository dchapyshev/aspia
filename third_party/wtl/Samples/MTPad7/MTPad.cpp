#include "stdatl.h"

#include <atlframe.h>
#include <atlctrls.h>
#include <atlctrlx.h>
#include <atldlgs.h>
#include <atlmisc.h>
#include <atlctrlw.h>
#include <atlprint.h>
#include <atlfind.h>
#include <atlribbon.h>
#include "finddlg.h"

#include "MTPadRibbon.h" 


#include "resource.h"

// Globals
#define WM_UPDATEROWCOL		(WM_USER + 1000)

LPCTSTR lpcstrMTPadRegKey = _T("Software\\Microsoft\\WTL Samples\\MTPad");

LPCTSTR lpcstrFilter = 
	_T("All Files (*.*)\0*.*\0")
	_T("Text Files (*.txt)\0*.txt\0")
	_T("C++ Files (*.cpp)\0*.cpp\0")
	_T("Include Files (*.h)\0*.h\0")
	_T("C Files (*.c)\0*.c\0")
	_T("Inline Files (*.inl)\0*.inl\0")
	_T("Ini Files (*.ini)\0*.ini\0")
	_T("Batch Files (*.bat)\0*.bat\0")
	_T("");

#include "view.h"
#include "aboutdlg.h"
#include "mainfrm.h"

#include "mtpad.h"

CAppModule _Module;

int WINAPI _tWinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPTSTR lpCmdLine, int nCmdShow)
{
	_Module.Init(NULL, hInstance);
	HINSTANCE hInstRich = ::LoadLibrary(CRichEditCtrl::GetLibraryName());

	CThreadManager mgr;
	int nRet = mgr.Run(lpCmdLine, nCmdShow);

	::FreeLibrary(hInstRich);
	_Module.Term();

	return nRet;
}
