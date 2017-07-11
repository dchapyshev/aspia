// [!output PROJECT_NAME].cpp : main source file for [!output PROJECT_NAME].exe
//

#include "stdafx.h"
[!if !WTL_USE_CPP_FILES]

#include <atlframe.h>
#include <atlctrls.h>
#include <atldlgs.h>
[!endif]

#include "resource.h"

[!if WTL_COM_SERVER]
// Note: Proxy/Stub Information
//		To build a separate proxy/stub DLL, 
//		run nmake -f [!output PROJECT_NAME]ps.mk in the project directory.
#include "initguid.h"
#include "[!output PROJECT_NAME].h"
#include "[!output PROJECT_NAME]_i.c"

[!endif]
[!if WTL_USE_VIEW]
#include "[!output WTL_VIEW_FILE].h"
[!endif]
[!if !WTL_APPTYPE_DLG_MODAL]
#include "aboutdlg.h"
[!endif]
[!if WTL_APPTYPE_DLG]
#include "[!output WTL_MAINDLG_FILE].h"
[!else]
#include "[!output WTL_FRAME_FILE].h"
[!endif]
[!if WTL_COM_SERVER]

CServerAppModule _Module;

BEGIN_OBJECT_MAP(ObjectMap)
END_OBJECT_MAP()
[!else]

CAppModule _Module;
[!endif]
[!if WTL_APPTYPE_DLG && !WTL_APPTYPE_DLG_MODAL]

int Run(LPTSTR /*lpstrCmdLine*/ = NULL, int nCmdShow = SW_SHOWNORMAL)
{
	CMessageLoop theLoop;
	_Module.AddMessageLoop(&theLoop);

	[!output WTL_MAINDLG_CLASS] dlgMain;

	if(dlgMain.Create(NULL) == NULL)
	{
		ATLTRACE(_T("Main dialog creation failed!\n"));
		return 0;
	}

[!if WTL_COM_SERVER]
	_Module.Lock();
[!endif]
	dlgMain.ShowWindow(nCmdShow);

	int nRet = theLoop.Run();

	_Module.RemoveMessageLoop();
	return nRet;
}
[!endif]
[!if !WTL_APPTYPE_DLG && WTL_APPTYPE_MTSDI]

class C[!output SAFE_PROJECT_NAME]ThreadManager
{
public:
	// thread init param
	struct _RunData
	{
		LPTSTR lpstrCmdLine;
		int nCmdShow;
	};

	// thread proc
	static DWORD WINAPI RunThread(LPVOID lpData)
	{
		CMessageLoop theLoop;
		_Module.AddMessageLoop(&theLoop);

		_RunData* pData = (_RunData*)lpData;
		[!output WTL_FRAME_CLASS] wndFrame;

		if(wndFrame.CreateEx() == NULL)
		{
			ATLTRACE(_T("Frame window creation failed!\n"));
			return 0;
		}

[!if WTL_COM_SERVER]
		_Module.Lock();
[!endif]
		wndFrame.ShowWindow(pData->nCmdShow);
		::SetForegroundWindow(wndFrame);	// Win95 needs this
		delete pData;

		int nRet = theLoop.Run();

		_Module.RemoveMessageLoop();
		return nRet;
	}

	DWORD m_dwCount;
	HANDLE m_arrThreadHandles[MAXIMUM_WAIT_OBJECTS - 1];

	C[!output SAFE_PROJECT_NAME]ThreadManager() : m_dwCount(0)
	{ }

// Operations
	DWORD AddThread(LPTSTR lpstrCmdLine, int nCmdShow)
	{
		if(m_dwCount == (MAXIMUM_WAIT_OBJECTS - 1))
		{
			::MessageBox(NULL, _T("ERROR: Cannot create ANY MORE threads!!!"), _T("[!output PROJECT_NAME]"), MB_OK);
			return 0;
		}

		_RunData* pData = new _RunData;
		pData->lpstrCmdLine = lpstrCmdLine;
		pData->nCmdShow = nCmdShow;
		DWORD dwThreadID;
		HANDLE hThread = ::CreateThread(NULL, 0, RunThread, pData, 0, &dwThreadID);
		if(hThread == NULL)
		{
			::MessageBox(NULL, _T("ERROR: Cannot create thread!!!"), _T("[!output PROJECT_NAME]"), MB_OK);
			return 0;
		}

		m_arrThreadHandles[m_dwCount] = hThread;
		m_dwCount++;
		return dwThreadID;
	}

	void RemoveThread(DWORD dwIndex)
	{
		::CloseHandle(m_arrThreadHandles[dwIndex]);
		if(dwIndex != (m_dwCount - 1))
			m_arrThreadHandles[dwIndex] = m_arrThreadHandles[m_dwCount - 1];
		m_dwCount--;
	}

	int Run(LPTSTR lpstrCmdLine, int nCmdShow)
	{
		MSG msg;
		// force message queue to be created
		::PeekMessage(&msg, NULL, WM_USER, WM_USER, PM_NOREMOVE);

		AddThread(lpstrCmdLine, nCmdShow);

		int nRet = m_dwCount;
		DWORD dwRet;
		while(m_dwCount > 0)
		{
			dwRet = ::MsgWaitForMultipleObjects(m_dwCount, m_arrThreadHandles, FALSE, INFINITE, QS_ALLINPUT);

			if(dwRet == 0xFFFFFFFF)
			{
				::MessageBox(NULL, _T("ERROR: Wait for multiple objects failed!!!"), _T("[!output PROJECT_NAME]"), MB_OK);
			}
			else if(dwRet >= WAIT_OBJECT_0 && dwRet <= (WAIT_OBJECT_0 + m_dwCount - 1))
			{
				RemoveThread(dwRet - WAIT_OBJECT_0);
			}
			else if(dwRet == (WAIT_OBJECT_0 + m_dwCount))
			{
				if(::PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
				{
					if(msg.message == WM_USER)
						AddThread(_T(""), SW_SHOWNORMAL);
				}
			}
			else
			{
				::MessageBeep((UINT)-1);
			}
		}

		return nRet;
	}
};
[!endif]
[!if !WTL_APPTYPE_DLG && !WTL_APPTYPE_MTSDI]

int Run(LPTSTR /*lpstrCmdLine*/ = NULL, int nCmdShow = SW_SHOWNORMAL)
{
	CMessageLoop theLoop;
	_Module.AddMessageLoop(&theLoop);

	[!output WTL_FRAME_CLASS] wndMain;

	if(wndMain.CreateEx() == NULL)
	{
		ATLTRACE(_T("Main window creation failed!\n"));
		return 0;
	}

[!if WTL_COM_SERVER]
	_Module.Lock();
[!endif]
	wndMain.ShowWindow(nCmdShow);

	int nRet = theLoop.Run();

	_Module.RemoveMessageLoop();
	return nRet;
}
[!endif]

[!if !WTL_APPTYPE_DLG_MODAL]
int WINAPI _tWinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPTSTR lpstrCmdLine, int nCmdShow)
[!else]
int WINAPI _tWinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPTSTR /*lpstrCmdLine*/, int /*nCmdShow*/)
[!endif]
{
[!if WTL_USE_SINGLE_APP_INSTANCE]
	HRESULT hRes = [!output WTL_FRAME_CLASS]::ActivatePreviousInstance(hInstance);
	if(FAILED(hRes) || S_FALSE == hRes)
	{
		return hRes;
	}

	hRes = ::CoInitializeEx(NULL, COINIT_MULTITHREADED);
[!else]
	HRESULT hRes = ::CoInitializeEx(NULL, COINIT_MULTITHREADED);
[!endif]
	ATLASSERT(SUCCEEDED(hRes));

	// Calling AtlInitCommonControls is not necessary to utilize picture,
	// static text, edit box, group box, button, check box, radio button, 
	// combo box, list box, or the horizontal and vertical scroll bars.
	// Calling AtlInitCommonControls with 0 is required to utilize the spin, 
	// progress, slider, list, tree, and tab controls.
	// Adding the ICC_DATE_CLASSES flag is required to initialize the 
	// date time picker and month calendar controls.
	// Add additional flags to support additoinal controls not mentioned above.
	AtlInitCommonControls(ICC_DATE_CLASSES);
[!if WTL_COM_SERVER]

	hRes = _Module.Init(ObjectMap, hInstance);
	ATLASSERT(SUCCEEDED(hRes));

[!else]

	hRes = _Module.Init(NULL, hInstance);
	ATLASSERT(SUCCEEDED(hRes));

[!endif]
[!if WTL_ENABLE_AX]
	AtlAxWinInit();

[!endif]
[!if WTL_COM_SERVER]
	// Parse command line, register or unregister or run the server
	int nRet = 0;
	TCHAR szTokens[] = _T("-/");
	bool bRun = true;
	bool bAutomation = false;

	LPCTSTR lpszToken = _Module.FindOneOf(::GetCommandLine(), szTokens);
	while(lpszToken != NULL)
	{
		if(lstrcmpi(lpszToken, _T("UnregServer")) == 0)
		{
			_Module.UpdateRegistryFromResource(IDR_[!output UPPERCASE_SAFE_PROJECT_NAME], FALSE);
			nRet = _Module.UnregisterServer(TRUE);
			bRun = false;
			break;
		}
		else if(lstrcmpi(lpszToken, _T("RegServer")) == 0)
		{
			_Module.UpdateRegistryFromResource(IDR_[!output UPPERCASE_SAFE_PROJECT_NAME], TRUE);
			nRet = _Module.RegisterServer(TRUE);
			bRun = false;
			break;
		}
		else if((lstrcmpi(lpszToken, _T("Automation")) == 0) ||
			(lstrcmpi(lpszToken, _T("Embedding")) == 0))
		{
			bAutomation = true;
			break;
		}
		lpszToken = _Module.FindOneOf(lpszToken, szTokens);
	}

	if(bRun)
	{
		_Module.StartMonitor();
		hRes = _Module.RegisterClassObjects(CLSCTX_LOCAL_SERVER, REGCLS_MULTIPLEUSE | REGCLS_SUSPENDED);
		ATLASSERT(SUCCEEDED(hRes));
		hRes = ::CoResumeClassObjects();
		ATLASSERT(SUCCEEDED(hRes));

		if(bAutomation)
		{
			CMessageLoop theLoop;
			nRet = theLoop.Run();
		}
		else
[!if WTL_APPTYPE_DLG_MODAL]
		{
			[!output WTL_MAINDLG_CLASS] dlgMain;
			nRet = dlgMain.DoModal();
		}
[!else]
[!if WTL_APPTYPE_MTSDI]
		{
			C[!output SAFE_PROJECT_NAME]ThreadManager mgr;
			nRet = mgr.Run(lpstrCmdLine, nCmdShow);
		}
[!else]
		{
			nRet = Run(lpstrCmdLine, nCmdShow);
		}
[!endif]
[!endif]

		_Module.RevokeClassObjects();
		::Sleep(_Module.m_dwPause);
	}
[!else]
[!if WTL_APPTYPE_DLG_MODAL]
	int nRet = 0;
	// BLOCK: Run application
	{
		[!output WTL_MAINDLG_CLASS] dlgMain;
		nRet = dlgMain.DoModal();
	}
[!else]
[!if WTL_APPTYPE_MTSDI]
	int nRet = 0;
	// BLOCK: Run application
	{
		C[!output SAFE_PROJECT_NAME]ThreadManager mgr;
		nRet = mgr.Run(lpstrCmdLine, nCmdShow);
	}
[!else]
	int nRet = Run(lpstrCmdLine, nCmdShow);
[!endif]
[!endif]
[!endif]
[!if WTL_USE_VIEW]
[!endif]

	_Module.Term();
	::CoUninitialize();

	return nRet;
}
