// [!output WTL_FRAME_FILE].h : interface of the [!output WTL_FRAME_CLASS] class
//
/////////////////////////////////////////////////////////////////////////////

#pragma once

class [!output WTL_FRAME_CLASS] : public [!output WTL_FRAME_BASE_CLASS]<[!output WTL_FRAME_CLASS]>, public CUpdateUI<[!output WTL_FRAME_CLASS]>,
		public CMessageFilter, public CIdleHandler
{
public:
	DECLARE_FRAME_WND_CLASS(NULL, IDR_MAINFRAME)

[!if WTL_APPTYPE_SDI || WTL_APPTYPE_MTSDI]
[!if WTL_USE_VIEW]
	[!output WTL_VIEW_CLASS] m_view;

[!endif]
[!endif]
[!if WTL_USE_CPP_FILES]
	virtual BOOL PreTranslateMessage(MSG* pMsg);
[!else]
	virtual BOOL PreTranslateMessage(MSG* pMsg)
	{
[!if WTL_USE_VIEW]
		if([!output WTL_FRAME_BASE_CLASS]<[!output WTL_FRAME_CLASS]>::PreTranslateMessage(pMsg))
			return TRUE;

		return m_view.PreTranslateMessage(pMsg);
[!else]
		return [!output WTL_FRAME_BASE_CLASS]<[!output WTL_FRAME_CLASS]>::PreTranslateMessage(pMsg);
[!endif]
	}

[!endif]
[!if WTL_USE_CPP_FILES]
	virtual BOOL OnIdle();
[!else]
	virtual BOOL OnIdle()
	{
[!if WTL_USE_TOOLBAR]
		UIUpdateToolBar();
[!endif]
		return FALSE;
	}
[!endif]

	BEGIN_UPDATE_UI_MAP([!output WTL_FRAME_CLASS])
[!if WTL_USE_TOOLBAR && !WTL_USE_AYGSHELL]
		UPDATE_ELEMENT(ID_VIEW_TOOLBAR, UPDUI_MENUPOPUP)
[!endif]
[!if WTL_USE_STATUSBAR]
		UPDATE_ELEMENT(ID_VIEW_STATUS_BAR, UPDUI_MENUPOPUP)
[!endif]
	END_UPDATE_UI_MAP()

	BEGIN_MSG_MAP([!output WTL_FRAME_CLASS])
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
[!if WTL_COM_SERVER]
		MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
[!endif]
[!if WTL_USE_SP03_COMPAT_MENUS]
		COMMAND_ID_HANDLER(ID_ACTION, OnAction)
[!endif]
		COMMAND_ID_HANDLER(ID_APP_EXIT, OnFileExit)
		COMMAND_ID_HANDLER(ID_FILE_NEW, OnFileNew)
[!if WTL_APPTYPE_MTSDI]
		COMMAND_ID_HANDLER(ID_FILE_NEW_WINDOW, OnFileNewWindow)
[!endif]
[!if WTL_USE_TOOLBAR && !WTL_USE_AYGSHELL]
		COMMAND_ID_HANDLER(ID_VIEW_TOOLBAR, OnViewToolBar)
[!endif]
[!if WTL_USE_STATUSBAR]
		COMMAND_ID_HANDLER(ID_VIEW_STATUS_BAR, OnViewStatusBar)
[!endif]
		COMMAND_ID_HANDLER(ID_APP_ABOUT, OnAppAbout)
		CHAIN_MSG_MAP(CUpdateUI<[!output WTL_FRAME_CLASS]>)
		CHAIN_MSG_MAP([!output WTL_FRAME_BASE_CLASS]<[!output WTL_FRAME_CLASS]>)
	END_MSG_MAP()

// Handler prototypes (uncomment arguments if needed):
//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

[!if WTL_USE_CPP_FILES]
	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
[!else]
	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
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
[!if !WTL_USE_AYGSHELL]
		UISetCheck(ID_VIEW_TOOLBAR, 1);
[!endif]
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

[!endif]
[!if WTL_COM_SERVER]
[!if WTL_USE_CPP_FILES]
	LRESULT OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
[!else]
	LRESULT OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
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
[!endif]
[!if WTL_USE_SP03_COMPAT_MENUS]
[!if WTL_USE_CPP_FILES]
	LRESULT OnAction(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
[!else]
	LRESULT OnAction(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		// TODO: add code

		return 0;
	}

[!endif]
[!endif]
[!if WTL_USE_CPP_FILES]
	LRESULT OnFileExit(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
[!else]
	LRESULT OnFileExit(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		PostMessage(WM_CLOSE);
		return 0;
	}

[!endif]
[!if WTL_USE_CPP_FILES]
	LRESULT OnFileNew(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
[!else]
	LRESULT OnFileNew(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		// TODO: add code to initialize document

		return 0;
	}

[!endif]
[!if WTL_APPTYPE_MTSDI]
[!if WTL_USE_CPP_FILES]
	LRESULT OnFileNewWindow(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
[!else]
	LRESULT OnFileNewWindow(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		::PostThreadMessage(_Module.m_dwMainThreadID, WM_USER, 0, 0L);
		return 0;
	}

[!endif]
[!endif]
[!if WTL_USE_TOOLBAR && !WTL_USE_AYGSHELL]
[!if WTL_USE_CPP_FILES]
	LRESULT OnViewToolBar(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
[!else]
	LRESULT OnViewToolBar(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		BOOL bVisible = !::IsWindowVisible(m_hWndToolBar);
		::ShowWindow(m_hWndToolBar, bVisible ? SW_SHOWNOACTIVATE : SW_HIDE);
		UISetCheck(ID_VIEW_TOOLBAR, bVisible);
		UpdateLayout();
		return 0;
	}

[!endif]
[!endif]
[!if WTL_USE_STATUSBAR]
[!if WTL_USE_CPP_FILES]
	LRESULT OnViewStatusBar(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
[!else]
	LRESULT OnViewStatusBar(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		BOOL bVisible = !::IsWindowVisible(m_hWndStatusBar);
		::ShowWindow(m_hWndStatusBar, bVisible ? SW_SHOWNOACTIVATE : SW_HIDE);
		UISetCheck(ID_VIEW_STATUS_BAR, bVisible);
		UpdateLayout();
		return 0;
	}

[!endif]
[!endif]
[!if WTL_USE_CPP_FILES]
	LRESULT OnAppAbout(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
[!else]
	LRESULT OnAppAbout(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		CAboutDlg dlg;
		dlg.DoModal();
		return 0;
	}
[!endif]
[!if WTL_USE_SINGLE_APP_INSTANCE]

[!if WTL_USE_CPP_FILES]
	static HRESULT ActivatePreviousInstance(HINSTANCE hInstance);
[!else]
	static HRESULT ActivatePreviousInstance(HINSTANCE hInstance)
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
[!endif]
};
