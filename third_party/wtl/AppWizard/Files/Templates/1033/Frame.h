// [!output WTL_FRAME_FILE].h : interface of the [!output WTL_FRAME_CLASS] class
//
/////////////////////////////////////////////////////////////////////////////

#pragma once

[!if WTL_APPTYPE_TABVIEW]
#define WINDOW_MENU_POSITION	3

[!endif]
class [!output WTL_FRAME_CLASS] : 
	public [!output WTL_FRAME_BASE_CLASS]<[!output WTL_FRAME_CLASS]>, 
[!if !WTL_USE_RIBBON]
	public CUpdateUI<[!output WTL_FRAME_CLASS]>,
[!endif]
	public CMessageFilter, public CIdleHandler
{
public:
	DECLARE_FRAME_WND_CLASS(NULL, IDR_MAINFRAME)

[!if WTL_APPTYPE_TABVIEW]
	CTabView m_view;
[!endif]
[!if WTL_APPTYPE_EXPLORER]
	CSplitterWindow m_splitter;
	CPaneContainer m_pane;
	CTreeViewCtrl m_treeview;
	[!output WTL_VIEW_CLASS] m_view;
[!endif]
[!if WTL_APPTYPE_SDI || WTL_APPTYPE_MTSDI]
[!if WTL_USE_VIEW]
	[!output WTL_VIEW_CLASS] m_view;
[!endif]
[!endif]
[!if WTL_USE_CMDBAR || WTL_USE_RIBBON]
[!if WTL_APPTYPE_MDI]
	CMDICommandBarCtrl m_CmdBar;
[!else]
	CCommandBarCtrl m_CmdBar;
[!endif]
[!endif]
[!if WTL_VIEWTYPE_LISTBOX || WTL_VIEWTYPE_EDIT || WTL_VIEWTYPE_RICHEDIT]
[!if !WTL_APPTYPE_MDI]
	CFont m_font;
[!endif]
[!endif]
[!if WTL_USE_RIBBON]

	//TODO: Declare ribbon controls

	// Ribbon control map
	BEGIN_RIBBON_CONTROL_MAP(CMainFrame)
	END_RIBBON_CONTROL_MAP()
[!endif]

[!if WTL_USE_CPP_FILES]
	virtual BOOL PreTranslateMessage(MSG* pMsg);
[!else]
	virtual BOOL PreTranslateMessage(MSG* pMsg)
	{
[!if WTL_APPTYPE_MDI]
		if([!output WTL_FRAME_BASE_CLASS]<[!output WTL_FRAME_CLASS]>::PreTranslateMessage(pMsg))
			return TRUE;

		HWND hWnd = MDIGetActive();
		if(hWnd != NULL)
			return (BOOL)::SendMessage(hWnd, WM_FORWARDMSG, 0, (LPARAM)pMsg);

		return FALSE;
[!else]
[!if WTL_USE_VIEW]
		if([!output WTL_FRAME_BASE_CLASS]<[!output WTL_FRAME_CLASS]>::PreTranslateMessage(pMsg))
			return TRUE;

		return m_view.PreTranslateMessage(pMsg);
[!else]
		return [!output WTL_FRAME_BASE_CLASS]<[!output WTL_FRAME_CLASS]>::PreTranslateMessage(pMsg);
[!endif]
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

[!if !WTL_RIBBON_SINGLE_UI]
	BEGIN_UPDATE_UI_MAP([!output WTL_FRAME_CLASS])
[!if WTL_USE_TOOLBAR]
		UPDATE_ELEMENT(ID_VIEW_TOOLBAR, UPDUI_MENUPOPUP)
[!endif]
[!if WTL_USE_STATUSBAR]
		UPDATE_ELEMENT(ID_VIEW_STATUS_BAR, UPDUI_MENUPOPUP)
[!endif]
[!if WTL_APPTYPE_EXPLORER]
		UPDATE_ELEMENT(ID_VIEW_TREEPANE, UPDUI_MENUPOPUP)
[!endif]
	END_UPDATE_UI_MAP()
[!endif]

	BEGIN_MSG_MAP([!output WTL_FRAME_CLASS])
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
		COMMAND_ID_HANDLER(ID_APP_EXIT, OnFileExit)
		COMMAND_ID_HANDLER(ID_FILE_NEW, OnFileNew)
[!if WTL_APPTYPE_MTSDI]
		COMMAND_ID_HANDLER(ID_FILE_NEW_WINDOW, OnFileNewWindow)
[!endif]
[!if WTL_USE_TOOLBAR]
		COMMAND_ID_HANDLER(ID_VIEW_TOOLBAR, OnViewToolBar)
[!endif]
[!if WTL_USE_STATUSBAR]
		COMMAND_ID_HANDLER(ID_VIEW_STATUS_BAR, OnViewStatusBar)
[!endif]
[!if WTL_RIBBON_DUAL_UI]
		COMMAND_ID_HANDLER(ID_VIEW_RIBBON, OnViewRibbon)
[!endif]
		COMMAND_ID_HANDLER(ID_APP_ABOUT, OnAppAbout)
[!if WTL_APPTYPE_MDI]
		COMMAND_ID_HANDLER(ID_WINDOW_CASCADE, OnWindowCascade)
		COMMAND_ID_HANDLER(ID_WINDOW_TILE_HORZ, OnWindowTile)
		COMMAND_ID_HANDLER(ID_WINDOW_ARRANGE, OnWindowArrangeIcons)
[!endif]
[!if WTL_APPTYPE_TABVIEW]
		COMMAND_ID_HANDLER(ID_WINDOW_CLOSE, OnWindowClose)
		COMMAND_ID_HANDLER(ID_WINDOW_CLOSE_ALL, OnWindowCloseAll)
		COMMAND_RANGE_HANDLER(ID_WINDOW_TABFIRST, ID_WINDOW_TABLAST, OnWindowActivate)
[!endif]
[!if WTL_APPTYPE_EXPLORER]
		COMMAND_ID_HANDLER(ID_VIEW_TREEPANE, OnViewTreePane)
		COMMAND_ID_HANDLER(ID_PANE_CLOSE, OnTreePaneClose)
[!endif]
[!if !WTL_USE_RIBBON]
		CHAIN_MSG_MAP(CUpdateUI<[!output WTL_FRAME_CLASS]>)
[!endif]
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
[!if WTL_RIBBON_DUAL_UI]
		bool bRibbonUI = RunTimeHelper::IsRibbonUIAvailable();
		if (bRibbonUI)
			UIAddMenu(GetMenu(), true);
		else
			CMenuHandle(GetMenu()).DeleteMenu(ID_VIEW_RIBBON, MF_BYCOMMAND);

[!else]
[!if WTL_RIBBON_SINGLE_UI]
		UIAddMenu(GetMenu(), true);
[!endif]
[!endif]
[!if WTL_USE_RIBBON && !WTL_USE_CMDBAR]
		m_CmdBar.Create(m_hWnd, rcDefault, NULL, WS_CHILD);
		m_CmdBar.AttachMenu(GetMenu());
		m_CmdBar.LoadImages(IDR_MAINFRAME);

[!endif]
[!if WTL_USE_TOOLBAR]
[!if WTL_USE_REBAR]
[!if WTL_USE_CMDBAR]
		// create command bar window
		HWND hWndCmdBar = m_CmdBar.Create(m_hWnd, rcDefault, NULL, ATL_SIMPLE_CMDBAR_PANE_STYLE);
		// attach menu
		m_CmdBar.AttachMenu(GetMenu());
		// load command bar images
		m_CmdBar.LoadImages(IDR_MAINFRAME);
		// remove old menu
		SetMenu(NULL);

[!endif]
		HWND hWndToolBar = CreateSimpleToolBarCtrl(m_hWnd, IDR_MAINFRAME, FALSE, ATL_SIMPLE_TOOLBAR_PANE_STYLE);

		CreateSimpleReBar(ATL_SIMPLE_REBAR_NOBORDER_STYLE);
[!if WTL_USE_CMDBAR]
		AddSimpleReBarBand(hWndCmdBar);
		AddSimpleReBarBand(hWndToolBar, NULL, TRUE);
[!else]
		AddSimpleReBarBand(hWndToolBar);
[!endif]
[!else]
		CreateSimpleToolBar();
[!endif]
[!endif]
[!if WTL_USE_STATUSBAR]

		CreateSimpleStatusBar();
[!endif]
[!if WTL_APPTYPE_MDI]

		CreateMDIClient();
[!if WTL_USE_CMDBAR]
		m_CmdBar.SetMDIClient(m_hWndMDIClient);
[!endif]
[!endif]
[!if WTL_APPTYPE_SDI || WTL_APPTYPE_MTSDI]
[!if WTL_USE_VIEW]
[!if WTL_VIEWTYPE_FORM]

		m_hWndClient = m_view.Create(m_hWnd);
[!else]
[!if WTL_VIEWTYPE_HTML]

		//TODO: Replace with a URL of your choice
		m_hWndClient = m_view.Create(m_hWnd, rcDefault, _T("http://www.microsoft.com"), [!output WTL_VIEW_STYLES], [!output WTL_VIEW_EX_STYLES]);
[!else]

		m_hWndClient = m_view.Create(m_hWnd, rcDefault, NULL, [!output WTL_VIEW_STYLES], [!output WTL_VIEW_EX_STYLES]);
[!endif]
[!if WTL_VIEWTYPE_LISTBOX || WTL_VIEWTYPE_EDIT || WTL_VIEWTYPE_RICHEDIT]
		m_font = AtlCreateControlFont();
		m_view.SetFont(m_font);
[!endif]
[!if WTL_VIEWTYPE_SCROLL]
		// replace with appropriate values for the app
		m_view.SetScrollSize(2000, 1000);
[!endif]
[!endif]
[!endif]
[!endif]
[!if WTL_APPTYPE_TABVIEW]

		m_hWndClient = m_view.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, WS_EX_CLIENTEDGE);
[!if WTL_VIEWTYPE_LISTBOX || WTL_VIEWTYPE_EDIT || WTL_VIEWTYPE_RICHEDIT]

		m_font = AtlCreateControlFont();
[!endif]
[!endif]
[!if WTL_APPTYPE_EXPLORER]

		m_hWndClient = m_splitter.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);

		m_pane.SetPaneContainerExtendedStyle(PANECNT_NOBORDER);
		m_pane.Create(m_splitter, _T("Tree"), WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);
		m_treeview.Create(m_pane, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | TVS_HASLINES | TVS_LINESATROOT | TVS_HASBUTTONS | TVS_SHOWSELALWAYS, WS_EX_CLIENTEDGE);
		m_pane.SetClient(m_treeview);
[!if WTL_VIEWTYPE_FORM]

		m_view.Create(m_splitter);
[!else]
[!if WTL_VIEWTYPE_HTML]

		//TODO: Replace with a URL of your choice
		m_view.Create(m_splitter, rcDefault, _T("http://www.microsoft.com"), [!output WTL_VIEW_STYLES], [!output WTL_VIEW_EX_STYLES]);
[!else]

		m_view.Create(m_splitter, rcDefault, NULL, [!output WTL_VIEW_STYLES], [!output WTL_VIEW_EX_STYLES]);
[!endif]
[!if WTL_VIEWTYPE_LISTBOX || WTL_VIEWTYPE_EDIT || WTL_VIEWTYPE_RICHEDIT]
		m_font = AtlCreateControlFont();
		m_view.SetFont(m_font);
[!endif]
[!if WTL_VIEWTYPE_SCROLL]
		// replace with appropriate values for the app
		m_view.SetScrollSize(2000, 1000);
[!endif]
[!endif]

		m_splitter.SetSplitterPanes(m_pane, m_view);
		UpdateLayout();
		m_splitter.SetSplitterPosPct(25);
[!endif]
[!if WTL_USE_TOOLBAR]
[!if WTL_USE_REBAR]

		UIAddToolBar(hWndToolBar);
[!else]

		UIAddToolBar(m_hWndToolBar);
[!endif]
		UISetCheck(ID_VIEW_TOOLBAR, 1);
[!endif]
[!if WTL_USE_STATUSBAR]
		UISetCheck(ID_VIEW_STATUS_BAR, 1);
[!endif]
[!if WTL_APPTYPE_EXPLORER]
		UISetCheck(ID_VIEW_TREEPANE, 1);
[!endif]

		// register object for message filtering and idle updates
		CMessageLoop* pLoop = _Module.GetMessageLoop();
		ATLASSERT(pLoop != NULL);
		pLoop->AddMessageFilter(this);
		pLoop->AddIdleHandler(this);

[!if WTL_USE_RIBBON]
[!if WTL_RIBBON_SINGLE_UI]
		ShowRibbonUI(true);

[!else]
		ShowRibbonUI(bRibbonUI);
		UISetCheck(ID_VIEW_RIBBON, bRibbonUI);

[!endif]
[!endif]
[!if WTL_APPTYPE_TABVIEW]
[!if WTL_USE_CMDBAR]
		CMenuHandle menuMain = m_CmdBar.GetMenu();
[!else]
		CMenuHandle menuMain = GetMenu();
[!endif]
		m_view.SetWindowMenu(menuMain.GetSubMenu(WINDOW_MENU_POSITION));

[!endif]
		return 0;
	}
[!endif]
[!if WTL_COM_SERVER]
[!if WTL_USE_CPP_FILES]
	LRESULT OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
[!else]

	LRESULT OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	{
[!if WTL_APPTYPE_MDI]
[!if WTL_USE_CMDBAR]
		m_CmdBar.AttachMenu(NULL);

[!endif]
[!endif]
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
[!else]
[!if WTL_USE_CPP_FILES]
	LRESULT OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
[!else]

	LRESULT OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
	{
[!if WTL_APPTYPE_MDI]
[!if WTL_USE_CMDBAR]
		m_CmdBar.AttachMenu(NULL);

[!endif]
[!endif]
		// unregister message filtering and idle updates
		CMessageLoop* pLoop = _Module.GetMessageLoop();
		ATLASSERT(pLoop != NULL);
		pLoop->RemoveMessageFilter(this);
		pLoop->RemoveIdleHandler(this);

		bHandled = FALSE;
		return 1;
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
[!if WTL_APPTYPE_TABVIEW]
		[!output WTL_VIEW_CLASS]* pView = new [!output WTL_VIEW_CLASS];
[!if WTL_VIEWTYPE_FORM]
		pView->Create(m_view);
[!else]
[!if WTL_VIEWTYPE_HTML]
		//TODO: Replace with a URL of your choice
		pView->Create(m_view, rcDefault, _T("http://www.microsoft.com"), [!output WTL_VIEW_STYLES], [!output WTL_VIEW_EX_STYLES]);
[!else]
		pView->Create(m_view, rcDefault, NULL, [!output WTL_VIEW_STYLES], [!output WTL_VIEW_EX_STYLES]);
[!endif]
[!if WTL_VIEWTYPE_LISTBOX || WTL_VIEWTYPE_EDIT || WTL_VIEWTYPE_RICHEDIT]
		pView->SetFont(m_font);
[!endif]
[!if WTL_VIEWTYPE_SCROLL]
		// replace with appropriate values for the app
		pView->SetScrollSize(2000, 1000);
[!endif]
[!endif]
		m_view.AddPage(pView->m_hWnd, _T("Document"));

[!endif]
[!if WTL_APPTYPE_MDI]
		[!output WTL_CHILD_FRAME_CLASS]* pChild = new [!output WTL_CHILD_FRAME_CLASS];
		pChild->CreateEx(m_hWndClient);

[!endif]
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
[!if WTL_USE_TOOLBAR]
[!if WTL_USE_CPP_FILES]
	LRESULT OnViewToolBar(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
[!else]

	LRESULT OnViewToolBar(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
[!if WTL_USE_REBAR]
		static BOOL bVisible = TRUE;	// initially visible
		bVisible = !bVisible;
		CReBarCtrl rebar = m_hWndToolBar;
[!if WTL_USE_CMDBAR]
		int nBandIndex = rebar.IdToIndex(ATL_IDW_BAND_FIRST + 1);	// toolbar is 2nd added band
[!else]
		int nBandIndex = rebar.IdToIndex(ATL_IDW_BAND_FIRST);	// toolbar is first 1st band
[!endif]
		rebar.ShowBand(nBandIndex, bVisible);
[!else]
		BOOL bVisible = !::IsWindowVisible(m_hWndToolBar);
		::ShowWindow(m_hWndToolBar, bVisible ? SW_SHOWNOACTIVATE : SW_HIDE);
[!endif]
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
[!if WTL_RIBBON_DUAL_UI]
[!if WTL_USE_CPP_FILES]
	LRESULT OnViewRibbon(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
[!else]

	LRESULT OnViewRibbon(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		ShowRibbonUI(!IsRibbonUI());
		UISetCheck(ID_VIEW_RIBBON, IsRibbonUI());
[!if !WTL_USE_CMDBAR]
		if (!IsRibbonUI())
			SetMenu(AtlLoadMenu(IDR_MAINFRAME));
[!endif]
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
[!if WTL_APPTYPE_MDI]
[!if WTL_USE_CPP_FILES]
	LRESULT OnWindowCascade(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
[!else]

	LRESULT OnWindowCascade(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		MDICascade();
		return 0;
	}
[!endif]
[!if WTL_USE_CPP_FILES]
	LRESULT OnWindowTile(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
[!else]

	LRESULT OnWindowTile(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		MDITile();
		return 0;
	}

[!endif]
[!if WTL_USE_CPP_FILES]
	LRESULT OnWindowArrangeIcons(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
[!else]

	LRESULT OnWindowArrangeIcons(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		MDIIconArrange();
		return 0;
	}
[!endif]
[!endif]
[!if WTL_APPTYPE_TABVIEW]
[!if WTL_USE_CPP_FILES]
	LRESULT OnWindowClose(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
[!else]

	LRESULT OnWindowClose(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		int nActivePage = m_view.GetActivePage();
		if(nActivePage != -1)
			m_view.RemovePage(nActivePage);
		else
			::MessageBeep((UINT)-1);

		return 0;
	}
[!endif]
[!if WTL_USE_CPP_FILES]
	LRESULT OnWindowCloseAll(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
[!else]

	LRESULT OnWindowCloseAll(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		m_view.RemoveAllPages();

		return 0;
	}
[!endif]
[!if WTL_USE_CPP_FILES]
LRESULT OnWindowActivate(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
[!else]

LRESULT OnWindowActivate(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int nPage = wID - ID_WINDOW_TABFIRST;
	m_view.SetActivePage(nPage);

	return 0;
}
[!endif]
[!endif]
[!if WTL_APPTYPE_EXPLORER]
[!if WTL_USE_CPP_FILES]
	LRESULT OnViewTreePane(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
[!else]

	LRESULT OnViewTreePane(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		bool bShow = (m_splitter.GetSinglePaneMode() != SPLIT_PANE_NONE);
		m_splitter.SetSinglePaneMode(bShow ? SPLIT_PANE_NONE : SPLIT_PANE_RIGHT);
		UISetCheck(ID_VIEW_TREEPANE, bShow);

		return 0;
	}
[!endif]
[!if WTL_USE_CPP_FILES]
	LRESULT OnTreePaneClose(WORD /*wNotifyCode*/, WORD /*wID*/, HWND hWndCtl, BOOL& /*bHandled*/);
[!else]

	LRESULT OnTreePaneClose(WORD /*wNotifyCode*/, WORD /*wID*/, HWND hWndCtl, BOOL& /*bHandled*/)
	{
		if(hWndCtl == m_pane.m_hWnd)
		{
			m_splitter.SetSinglePaneMode(SPLIT_PANE_RIGHT);
			UISetCheck(ID_VIEW_TREEPANE, 0);
		}

		return 0;
	}
[!endif]
[!endif]
};
