// [!output WTL_APPWND_FILE].h : interface of the [!output WTL_FRAME_CLASS] class
//
/////////////////////////////////////////////////////////////////////////////

#pragma once

class [!output WTL_FRAME_CLASS] : 
	public CFrameWindowImpl<[!output WTL_FRAME_CLASS]>, 
	public CUpdateUI<[!output WTL_FRAME_CLASS]>,
	public CAppWindow<[!output WTL_FRAME_CLASS]>,
[!if WTL_FULLSCREEN]
	public CFullScreenFrame<[!output WTL_FRAME_CLASS]>,
[!endif]
	public CMessageFilter, public CIdleHandler
{
public:
	DECLARE_APP_FRAME_CLASS(NULL, IDR_MAINFRAME, L"Software\\WTL\\[!output NICE_SAFE_PROJECT_NAME]")

[!if WTL_APPTYPE_SDI]
[!if WTL_USE_VIEW]
	[!output WTL_VIEW_CLASS] m_view;

[!endif]
[!endif]
	virtual BOOL PreTranslateMessage(MSG* pMsg)
	{
[!if WTL_USE_VIEW_CLASS || WTL_VIEWTYPE_AX]
		if(CFrameWindowImpl<[!output WTL_FRAME_CLASS]>::PreTranslateMessage(pMsg))
			return TRUE; 

[!if !WTL_USE_VIEW_CLASS && WTL_VIEWTYPE_AX]
		if((pMsg->message < WM_KEYFIRST || pMsg->message > WM_KEYLAST) &&
		   (pMsg->message < WM_MOUSEFIRST || pMsg->message > WM_MOUSELAST))
			return FALSE;

		HWND hWndCtl = ::GetFocus();
		if(IsChild(hWndCtl))
		{
			// find a direct child of the dialog from the window that has focus
			while(::GetParent(hWndCtl) != m_hWnd)
				hWndCtl = ::GetParent(hWndCtl);

			// give control a chance to translate this message
			if(::SendMessage(hWndCtl, WM_FORWARDMSG, 0, (LPARAM)pMsg) != 0)
				return TRUE;
		}

		return FALSE;
[!else]
		return m_view.IsWindow() ? m_view.PreTranslateMessage(pMsg) : FALSE;
[!endif]
[!else]
		return CFrameWindowImpl<[!output WTL_FRAME_CLASS]>::PreTranslateMessage(pMsg);
[!endif]
	}

// CAppWindow operations
	bool AppHibernate( bool bHibernate)
	{
		// Insert your code here or delete member if not relevant
		return bHibernate;
	}

	bool AppNewInstance( LPCTSTR lpstrCmdLine)
	{
		// Insert your code here or delete member if not relevant
		return false;
	}

	void AppSave()
	{
		CAppInfo info;
[!if WTL_FULLSCREEN]
		info.Save( m_bFullScreen, L"Full");
[!endif]
[!if WTL_USE_STATUSBAR]
		bool bStatus = (UIGetState(ID_VIEW_STATUS_BAR) & UPDUI_CHECKED) == UPDUI_CHECKED;
		info.Save(bStatus, L"Status");
[!endif]
		// Insert your code here
	}

//?sp
	void AppBackKey() 
	{
		::SHNavigateBack();
	}
//?end

	virtual BOOL OnIdle()
	{
[!if WTL_USE_VIEW && WTL_VIEWTYPE_PROPSHEET]
		if (!m_view.IsWindow() || m_view.SendMessage(PSM_GETCURRENTPAGEHWND) == NULL)
			PostMessage(WM_CLOSE);

[!endif]
		UIUpdateToolBar();
[!if WTL_USE_STATUSBAR]
		UIUpdateStatusBar();
[!endif]
		return FALSE;
	}

	BEGIN_UPDATE_UI_MAP([!output WTL_FRAME_CLASS])
[!if WTL_FULLSCREEN]
		UPDATE_ELEMENT(ID_VIEW_FULLSCREEN, UPDUI_MENUPOPUP)
[!endif]
[!if WTL_USE_STATUSBAR]
		UPDATE_ELEMENT(ID_VIEW_STATUS_BAR, UPDUI_MENUPOPUP)
[!endif]
	END_UPDATE_UI_MAP()

	BEGIN_MSG_MAP([!output WTL_FRAME_CLASS])
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		COMMAND_ID_HANDLER(ID_APP_EXIT, OnFileExit)
		COMMAND_ID_HANDLER(ID_FILE_NEW, OnFileNew)
[!if SMARTPHONE2003_UI_MODEL || !WTL_MENU_TYPE_2003]
		COMMAND_ID_HANDLER(ID_ACTION, OnAction)
[!endif]
[!if WTL_FULLSCREEN]
		COMMAND_ID_HANDLER(ID_VIEW_FULLSCREEN, OnFullScreen)
[!endif]
[!if WTL_USE_STATUSBAR]
		COMMAND_ID_HANDLER(ID_VIEW_STATUS_BAR, OnViewStatusBar)
[!endif]
		COMMAND_ID_HANDLER(ID_APP_ABOUT, OnAppAbout)
		CHAIN_MSG_MAP(CAppWindow<[!output WTL_FRAME_CLASS]>)
[!if WTL_FULLSCREEN]
		CHAIN_MSG_MAP(CFullScreenFrame<[!output WTL_FRAME_CLASS]>)
[!endif]
		CHAIN_MSG_MAP(CUpdateUI<[!output WTL_FRAME_CLASS]>)
		CHAIN_MSG_MAP(CFrameWindowImpl<[!output WTL_FRAME_CLASS]>)
	END_MSG_MAP()

// Handler prototypes (uncomment arguments if needed):
//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	{
		CAppInfo info;
[!if WTL_FULLSCREEN]

		// Full screen mode delayed restoration 
		bool bFull = false;
		info.Restore(bFull, L"Full");
		if (bFull)
			PostMessage(WM_COMMAND, ID_VIEW_FULLSCREEN);
[!endif]

[!if POCKETPC2003_UI_MODEL && SMARTPHONE2003_UI_MODEL && !WTL_MENU_TYPE_2005]
#ifdef WIN32_PLATFORM_PSPC // PPC
[!endif]
[!if POCKETPC2003_UI_MODEL]
[!if WTL_MENU_TYPE_BOTH]
		OSVERSIONINFO osvi;
		GetVersionEx(&osvi);
		if (osvi.dwMajorVersion >= 5)
			CreateSimpleCEMenuBar();
		else
			CreateSimpleCEMenuBar(IDR_MAINFRAME, 0, IDR_MAINFRAME, 7);
[!endif]
[!if WTL_MENU_TYPE_2003]
		CreateSimpleCEMenuBar(IDR_MAINFRAME, 0, IDR_MAINFRAME, 7);
[!endif]
[!endif]
[!if WTL_MENU_TYPE_2005]
		CreateSimpleCEMenuBar();
[!endif]
[!if POCKETPC2003_UI_MODEL && SMARTPHONE2003_UI_MODEL]
[!if WTL_MENU_TYPE_2005]
#ifdef WIN32_PLATFORM_WFSP // SmartPhone
[!else]
#else // SmartPhone
[!endif]
[!endif]
[!if SMARTPHONE2003_UI_MODEL]
[!if !WTL_MENU_TYPE_2005]
		CreateSimpleCEMenuBar();
[!endif]
		AtlActivateBackKey(m_hWndCECommandBar);
[!endif]
[!if POCKETPC2003_UI_MODEL && SMARTPHONE2003_UI_MODEL]
#endif 
[!endif]
		UIAddToolBar(m_hWndCECommandBar);
[!if WTL_USE_STATUSBAR]

		// StatusBar state restoration 
		bool bVisible = true;
		info.Restore(bVisible, L"Status");
		DWORD dwStyle = WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
		if (bVisible)
			dwStyle |= WS_VISIBLE;
		// StatusBar creation 
		CreateSimpleStatusBar(ATL_IDS_IDLEMESSAGE, dwStyle);
		UIAddStatusBar(m_hWndStatusBar);
		UISetCheck(ID_VIEW_STATUS_BAR, bVisible);
[!endif]
[!if WTL_USE_VIEW]

[!if WTL_VIEWTYPE_AX]
		//TODO: Replace with a ProgID of your choice
		m_hWndClient = m_view.Create(m_hWnd, rcDefault, _T("WMPlayer.OCX"), [!output WTL_VIEW_STYLES]);
[!else]
[!if WTL_VIEWTYPE_FORM || WTL_VIEWTYPE_PROPSHEET]
		m_hWndClient = m_view.Create(m_hWnd);
[!else]
		m_hWndClient = m_view.Create(m_hWnd, rcDefault, NULL, [!output WTL_VIEW_STYLES]);
[!endif]
[!endif]
[!endif]

		// register object for message filtering and idle updates
		CMessageLoop* pLoop = _Module.GetMessageLoop();
		ATLASSERT(pLoop != NULL);
		pLoop->AddMessageFilter(this);
		pLoop->AddIdleHandler(this);

		return 0;
	}

	LRESULT OnFileExit(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		PostMessage(WM_CLOSE);
		return 0;
	}

	LRESULT OnFileNew(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		// TODO: add code to initialize document

		return 0;
	}

[!if SMARTPHONE2003_UI_MODEL || !WTL_MENU_TYPE_2003]
	LRESULT OnAction(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		// TODO: add code

		return 0;
	}

[!endif]
[!if WTL_FULLSCREEN]
	LRESULT OnFullScreen(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		SetFullScreen( !m_bFullScreen );
		UISetCheck( ID_VIEW_FULLSCREEN, m_bFullScreen);
		return TRUE;
	}

[!endif]
[!if WTL_USE_STATUSBAR]
	LRESULT OnViewStatusBar(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		BOOL bVisible = !::IsWindowVisible(m_hWndStatusBar);
		::ShowWindow(m_hWndStatusBar, bVisible ? SW_SHOWNOACTIVATE : SW_HIDE);
		UISetCheck(ID_VIEW_STATUS_BAR, bVisible);
		UpdateLayout();
		return 0;
	}

[!endif]
	LRESULT OnAppAbout(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		CAboutDlg dlg;
[!if WTL_FULLSCREEN]
		FSDoModal(dlg);
[!else]
		dlg.DoModal();
[!endif]
		return 0;
	}
};
