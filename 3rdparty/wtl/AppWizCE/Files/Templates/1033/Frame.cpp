// [!output WTL_FRAME_FILE].cpp : implmentation of the [!output WTL_FRAME_CLASS] class
//
/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "resource.h"

#include "aboutdlg.h"
[!if WTL_USE_VIEW]
#include "[!output WTL_VIEW_FILE].h"
[!endif]
#include "[!output WTL_FRAME_FILE].h"

BOOL [!output WTL_FRAME_CLASS]::PreTranslateMessage(MSG* pMsg)
{
[!if WTL_USE_VIEW]
	if([!output WTL_FRAME_BASE_CLASS]<[!output WTL_FRAME_CLASS]>::PreTranslateMessage(pMsg))
		return TRUE;

	return m_view.PreTranslateMessage(pMsg);
[!else]
	return [!output WTL_FRAME_BASE_CLASS]<[!output WTL_FRAME_CLASS]>::PreTranslateMessage(pMsg);
[!endif]
}

BOOL [!output WTL_FRAME_CLASS]::OnIdle()
{
[!if WTL_USE_TOOLBAR]
	UIUpdateToolBar();
[!endif]
	return FALSE;
}

LRESULT [!output WTL_FRAME_CLASS]::OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
[!if WTL_USE_AYGSHELL]
[!if !WTL_USE_SP03_COMPAT_MENUS]
		CreateSimpleCEMenuBar(IDR_MAINFRAME);
[!else]
		CreateSimpleCEMenuBar();
[!endif]
[!else]
		CreateSimpleCECommandBar(MAKEINTRESOURCE(IDR_MAINFRAME));
[!endif]

[!if WTL_USE_TOOLBAR]
	CreateSimpleToolBar();
[!endif]
[!if WTL_USE_STATUSBAR]

	CreateSimpleStatusBar();
[!endif]
[!if WTL_APPTYPE_SDI || WTL_APPTYPE_MTSDI]
[!if WTL_USE_VIEW]
[!if WTL_VIEWTYPE_FORM]

	m_hWndClient = m_view.Create(m_hWnd);
[!else]
[!if WTL_VIEWTYPE_HTML]

	//TODO: Replace with a URL of your choice
	m_hWndClient = m_view.Create(m_hWnd, rcDefault, _T("http://www.microsoft.com"), [!output WTL_VIEW_STYLES]);
[!else]

	m_hWndClient = m_view.Create(m_hWnd, rcDefault, NULL, [!output WTL_VIEW_STYLES]);
[!endif]
[!endif]
[!endif]
[!endif]
[!if WTL_USE_TOOLBAR]

	UIAddToolBar(m_hWndToolBar);
	UISetCheck(ID_VIEW_TOOLBAR, 1);
[!endif]
[!if WTL_USE_STATUSBAR]
	UISetCheck(ID_VIEW_STATUS_BAR, 1);
[!endif]

	// register object for message filtering and idle updates
	CMessageLoop* pLoop = _Module.GetMessageLoop();
	ATLASSERT(pLoop != NULL);
	pLoop->AddMessageFilter(this);
	pLoop->AddIdleHandler(this);

	return 0;
}

[!if WTL_COM_SERVER]
LRESULT [!output WTL_FRAME_CLASS]::OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	// unregister message filtering and idle updates
	CMessageLoop* pLoop = _Module.GetMessageLoop();
	ATLASSERT(pLoop != NULL);
	pLoop->RemoveMessageFilter(this);
	pLoop->RemoveIdleHandler(this);
	// if UI is the last thread, no need to wait
	if(_Module.GetLockCount() == 1)
	{
		_Module.m_dwTimeOut = 0L;
		_Module.m_dwPause = 0L;
	}
	_Module.Unlock();
[!if WTL_APPTYPE_MTSDI]
	::PostQuitMessage(1);
[!endif]
	return 0;
}

[!endif]
[!if WTL_USE_SP03_COMPAT_MENUS]
LRESULT [!output WTL_FRAME_CLASS]::OnAction(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	// TODO: add code

	return 0;
}

[!endif]
LRESULT [!output WTL_FRAME_CLASS]::OnFileExit(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	PostMessage(WM_CLOSE);
	return 0;
}

LRESULT [!output WTL_FRAME_CLASS]::OnFileNew(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	// TODO: add code to initialize document

	return 0;
}

[!if WTL_APPTYPE_MTSDI]
LRESULT [!output WTL_FRAME_CLASS]::OnFileNewWindow(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	::PostThreadMessage(_Module.m_dwMainThreadID, WM_USER, 0, 0L);
	return 0;
}

[!endif]
[!if WTL_USE_TOOLBAR]
LRESULT [!output WTL_FRAME_CLASS]::OnViewToolBar(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	BOOL bVisible = !::IsWindowVisible(m_hWndToolBar);
	::ShowWindow(m_hWndToolBar, bVisible ? SW_SHOWNOACTIVATE : SW_HIDE);
	UISetCheck(ID_VIEW_TOOLBAR, bVisible);
	UpdateLayout();
	return 0;
}

[!endif]
[!if WTL_USE_STATUSBAR]
LRESULT [!output WTL_FRAME_CLASS]::OnViewStatusBar(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	BOOL bVisible = !::IsWindowVisible(m_hWndStatusBar);
	::ShowWindow(m_hWndStatusBar, bVisible ? SW_SHOWNOACTIVATE : SW_HIDE);
	UISetCheck(ID_VIEW_STATUS_BAR, bVisible);
	UpdateLayout();
	return 0;
}

[!endif]
LRESULT [!output WTL_FRAME_CLASS]::OnAppAbout(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	CAboutDlg dlg;
	dlg.DoModal();
	return 0;
}
[!if WTL_USE_SINGLE_APP_INSTANCE]

HRESULT [!output WTL_FRAME_CLASS]::ActivatePreviousInstance(HINSTANCE hInstance)
{
	CFrameWndClassInfo& classInfo = [!output WTL_FRAME_CLASS]::GetWndClassInfo();
	ATLVERIFY(::LoadString(hInstance, IDR_MAINFRAME, classInfo.m_szAutoName, sizeof(classInfo.m_szAutoName)/sizeof(classInfo.m_szAutoName[0])));
	classInfo.m_wc.lpszClassName = classInfo.m_szAutoName;
	const TCHAR* pszClass = classInfo.m_wc.lpszClassName;
	if(pszClass == NULL || *pszClass == _T('\0'))
	{
		return E_FAIL;
	}

	// Orginally 500ms in SmartPhone 2003 App Wizard generated code
	// A lower value will result in a more responsive start-up of 
	// the existing instance or termination of this instance.
	const DWORD dRetryInterval = 100; 

	// Orginally 5 in SmartPhone 2003 App Wizard generated code
	// Multiplied by 5, since wait time was divided by 5.
	const int iMaxRetries = 25;

	for(int i = 0; i < iMaxRetries; ++i)
	{
		// Don't need ownership of the mutex
		HANDLE hMutex = CreateMutex(NULL, FALSE, pszClass);

		DWORD dw = GetLastError();

		if(hMutex == NULL)
		{
			// ERROR_INVALID_HANDLE - A non-mutex object with this name already exists.
			HRESULT hr = (dw == ERROR_INVALID_HANDLE) ? E_INVALIDARG : E_FAIL;
			return hr;
		}

		// If the mutex already exists, then there should be another instance running
		if(dw == ERROR_ALREADY_EXISTS)
		{
			// Just needed the error result, in this case, so close the handle.
			CloseHandle(hMutex);

			// Try to find the other instance, don't need to close HWND's.
			// Don't check title in case it is changed by app after init.
			HWND hwnd = FindWindow(pszClass, NULL);

			if(hwnd == NULL)
			{
				// It's possible that the other istance is in the process of starting up or shutting down.
				// So wait a bit and try again.
				Sleep(dRetryInterval);
				continue;
			}
			else
			{
				// Set the previous instance as the foreground window

				// The "| 0x1" in the code below activates the correct owned window 
				// of the previous instance's main window according to the SmartPhone 2003
				// wizard generated code.
				if(SetForegroundWindow(reinterpret_cast<HWND>(reinterpret_cast<ULONG>(hwnd) | 0x1)) != 0)
				{
					// S_FALSE indicates that another instance was activated, so this instance should terminate.
					return S_FALSE;
				}
			}
		}
		else
		{
			// This is the first istance, so return S_OK.
			// Don't close the mutext handle here.
			// Do it on app shutdown instead.
			return S_OK;
		}
	}

	// The mutex was created by another instance, but it's window couldn't be brought
	// to the foreground, so ssume  it's not a invalid instance (not this app, hung, etc.)
	// and let this one start.
	return S_OK;
}
[!endif]
