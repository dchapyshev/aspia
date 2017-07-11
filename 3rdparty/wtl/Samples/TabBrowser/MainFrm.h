// MainFrm.h : interface of the CMainFrame class
//
/////////////////////////////////////////////////////////////////////////////

#pragma once

#define WINDOW_MENU_POSITION	3


class CMainFrame : public CFrameWindowImpl<CMainFrame>, public CUpdateUI<CMainFrame>,
		public CMessageFilter, public CIdleHandler
{
public:
	DECLARE_FRAME_WND_CLASS(_T("TabBrowser_MainFrame"), IDR_MAINFRAME)

// Data members
	CCommandBarCtrl m_CmdBar;
	CCustomTabView m_view;
	CAddressComboBox m_cb;

	CString m_strHomePage;

// CMessageFilter
	virtual BOOL PreTranslateMessage(MSG* pMsg)
	{
		if(ProcessComboReturnKey(pMsg) != FALSE)
			return TRUE;

		if(m_view.PreTranslateMessage(pMsg) != FALSE)
			return TRUE;

		return CFrameWindowImpl<CMainFrame>::PreTranslateMessage(pMsg);
	}

	BOOL ProcessComboReturnKey(MSG* pMsg)
	{
		BOOL bRet = FALSE;

		if(IsPageActive() && (pMsg->hwnd == m_cb.m_hWnd || ::IsChild(m_cb.m_hWnd, pMsg->hwnd)))
		{
			if((pMsg->message == WM_KEYDOWN || pMsg->message == WM_SYSKEYDOWN) && pMsg->wParam == VK_RETURN)
			{
				CString str;
				int nLen = m_cb.GetWindowTextLength();
				m_cb.GetWindowText(str.GetBuffer(nLen), nLen + 1);
				str.ReleaseBuffer();
				str.TrimLeft();
				str.TrimRight();

				if(!str.IsEmpty())
				{
					if(pMsg->message == WM_SYSKEYDOWN)
					{
						OpenPage(str, true);
					}
					if(GetKeyState(VK_CONTROL) < 0)
					{
						CString strExp;
						strExp.Format(_T("http://www.%s.com"), (LPCTSTR)str);

						m_cb.SetWindowText(strExp);
						OpenPage(strExp, false);
					}
					else
					{
						OpenPage(str, false);
					}
				}

				bRet = TRUE;
			}
		}

		return bRet;
	}

// CIdleHandler
	virtual BOOL OnIdle()
	{
		bool bActive = IsPageActive();

		bool bCanGoBack = false;
		bool bCanGoForward = false;

		if(bActive)
		{
			CBrowserView* pView = (CBrowserView*)m_view.GetPageData(m_view.GetActivePage());
			bCanGoBack = pView->m_bCanGoBack;
			bCanGoForward = pView->m_bCanGoForward;
		}

		UIEnable(ID_BROWSER_BACK, bActive && bCanGoBack);
		UIEnable(ID_BROWSER_FORWARD, bActive && bCanGoForward);
		UIEnable(ID_BROWSER_STOP, bActive);
		UIEnable(ID_BROWSER_REFRESH, bActive);
		UIEnable(ID_BROWSER_HOME, bActive);
		UIEnable(ID_BROWSER_SEARCH, bActive);

		UIEnable(ID_WINDOW_CLOSE, bActive);
		UIEnable(ID_WINDOW_CLOSE_ALL, bActive);

		UIEnable(ID_WINDOW_SHOW_VIEWS, bActive);

		UIEnable(ID_GO, bActive);

		UIUpdateToolBar();

		m_cb.EnableWindow(bActive ? TRUE : FALSE);

		return FALSE;
	}

// Methods
	bool GetHomePage()
	{
		LPCTSTR lpstrHomePageRegKey = _T("Software\\Microsoft\\Internet Explorer\\Main");
		LPCTSTR lpstrHomePageRegValue = _T("Start Page");

		CRegKeyEx rk;
		LONG lRet = rk.Open(HKEY_CURRENT_USER, lpstrHomePageRegKey);
		ATLASSERT(lRet == ERROR_SUCCESS);
		if(lRet != ERROR_SUCCESS)
			return false;

		ULONG ulLength = 0;
		lRet = rk.QueryStringValue(lpstrHomePageRegValue, NULL, &ulLength);
		if(lRet == ERROR_SUCCESS)
			lRet = rk.QueryStringValue(lpstrHomePageRegValue, m_strHomePage.GetBuffer(ulLength), &ulLength);
		ATLASSERT(lRet == ERROR_SUCCESS);
		m_strHomePage.ReleaseBuffer();

		return (lRet == ERROR_SUCCESS);
	}

	bool IsPageActive() const
	{
		return (m_view.GetActivePage() != -1);
	}

	IWebBrowser2* GetBrowser(int nPage) const
	{
		CAxWindow wnd = m_view.GetPageHWND(nPage);

		CComPtr<IWebBrowser2> spWebBrowser;
		HRESULT hRet = wnd.QueryControl(IID_IWebBrowser2, (void**)&spWebBrowser);
		hRet;   // avoid level 4 warning
		ATLASSERT(SUCCEEDED(hRet));

		return spWebBrowser;
	}

	void OpenPage(LPCTSTR lpstrURL, bool bNewTab)
	{
		CString strLoading;
		strLoading.LoadString(IDS_LOADING);

		bool bSuccess = false;

		if(bNewTab)
		{
			CBrowserView* pView = new CBrowserView;
			pView->Create(m_view, rcDefault, lpstrURL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_VSCROLL | WS_HSCROLL);
			if(pView->IsWindow())
				bSuccess = m_view.AddPage(pView->m_hWnd, strLoading, 0, pView);
		}
		else
		{
			m_view.SetPageTitle(m_view.GetActivePage(), strLoading);

			CComPtr<IWebBrowser2> spWebBrowser = GetBrowser(m_view.GetActivePage());
			if(spWebBrowser != NULL)
			{
				CComVariant vtURL(lpstrURL);
				CComVariant vt;
				HRESULT hRet = spWebBrowser->Navigate2(&vtURL, &vt, &vt, &vt, &vt);
				bSuccess = SUCCEEDED(hRet);
			}
		}

		if(!bSuccess)
		{
			CString strError;
			strError.LoadString(IDS_OPEN_ERROR);
			strError += lpstrURL;
			CString strTitle;
			strTitle.LoadString(IDR_MAINFRAME);
			MessageBox(strError, strTitle, MB_OK | MB_ICONERROR);
		}
	}

	void BuildLinksToolbar(HWND hWndToolBar)
	{
		CToolBarCtrl tb = hWndToolBar;

		tb.SetExtendedStyle(TBSTYLE_EX_MIXEDBUTTONS);

		// add links buttons
		CString strTitle;
		strTitle.LoadString(ID_LINKS_MICROSOFT);
		tb.AddButton(ID_LINKS_MICROSOFT, BTNS_BUTTON | BTNS_AUTOSIZE | BTNS_SHOWTEXT, TBSTATE_ENABLED, 0, strTitle, 0);
		strTitle.LoadString(ID_LINKS_MSDN);
		tb.AddButton(ID_LINKS_MSDN, BTNS_BUTTON | BTNS_AUTOSIZE | BTNS_SHOWTEXT, TBSTATE_ENABLED, 0, strTitle, 0);
		strTitle.LoadString(ID_LINKS_LIVE);
		tb.AddButton(ID_LINKS_LIVE, BTNS_BUTTON | BTNS_AUTOSIZE | BTNS_SHOWTEXT, TBSTATE_ENABLED, 0, strTitle, 0);
		strTitle.LoadString(ID_LINKS_UPDATE);
		tb.AddButton(ID_LINKS_UPDATE, BTNS_BUTTON | BTNS_AUTOSIZE | BTNS_SHOWTEXT, TBSTATE_ENABLED, 0, strTitle, 0);
		strTitle.LoadString(ID_LINKS_WTL);
		tb.AddButton(ID_LINKS_WTL, BTNS_BUTTON | BTNS_AUTOSIZE | BTNS_SHOWTEXT, TBSTATE_ENABLED, 0, strTitle, 0);

		// delete initial button (not used)
		tb.DeleteButton(0);
	}

	void DeactivateHTMLPage()
	{
		if(IsPageActive())
		{
			CAxWindow wnd = m_view.GetPageHWND(m_view.GetActivePage());
			CComPtr<IOleInPlaceObject> spInPlaceObject;
			HRESULT hRet = wnd.QueryControl(IID_IOleInPlaceObject, (void**)&spInPlaceObject);
			ATLASSERT(SUCCEEDED(hRet));
			if(spInPlaceObject != NULL)
			{
				hRet = spInPlaceObject->UIDeactivate();
				ATLASSERT(SUCCEEDED(hRet));
			}
		}
	}

// Update UI map
	BEGIN_UPDATE_UI_MAP(CMainFrame)
		UPDATE_ELEMENT(ID_VIEW_TOOLBAR, UPDUI_MENUPOPUP)
		UPDATE_ELEMENT(ID_VIEW_STATUS_BAR, UPDUI_MENUPOPUP)
		UPDATE_ELEMENT(ID_VIEW_ADDRESS_BAR, UPDUI_MENUPOPUP)
		UPDATE_ELEMENT(ID_VIEW_LINKS_BAR, UPDUI_MENUPOPUP)
		UPDATE_ELEMENT(ID_VIEW_LOCK_TOOLBARS, UPDUI_MENUPOPUP)

		UPDATE_ELEMENT(ID_BROWSER_BACK, UPDUI_TOOLBAR | UPDUI_MENUPOPUP)
		UPDATE_ELEMENT(ID_BROWSER_FORWARD, UPDUI_TOOLBAR | UPDUI_MENUPOPUP)
		UPDATE_ELEMENT(ID_BROWSER_STOP, UPDUI_TOOLBAR | UPDUI_MENUPOPUP)
		UPDATE_ELEMENT(ID_BROWSER_REFRESH, UPDUI_TOOLBAR | UPDUI_MENUPOPUP)
		UPDATE_ELEMENT(ID_BROWSER_HOME, UPDUI_TOOLBAR | UPDUI_MENUPOPUP)
		UPDATE_ELEMENT(ID_BROWSER_SEARCH, UPDUI_TOOLBAR | UPDUI_MENUPOPUP)

		UPDATE_ELEMENT(ID_GO, UPDUI_TOOLBAR)

		UPDATE_ELEMENT(ID_WINDOW_CLOSE, UPDUI_TOOLBAR | UPDUI_MENUPOPUP)
		UPDATE_ELEMENT(ID_WINDOW_CLOSE_ALL, UPDUI_TOOLBAR | UPDUI_MENUPOPUP)
	END_UPDATE_UI_MAP()

// Message map and handlers
	BEGIN_MSG_MAP(CMainFrame)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
		MESSAGE_HANDLER(WM_CONTEXTMENU, OnContextMenu)

		MESSAGE_HANDLER(WM_BROWSERTITLECHANGE, OnBrowserTitleChange)
		MESSAGE_HANDLER(WM_BROWSERDOCUMENTCOMPLETE, OnBrowserDocumentComplete)
		MESSAGE_HANDLER(WM_BROWSERSTATUSTEXTCHANGE, OnBrowserStatusTextChange)

		MESSAGE_HANDLER(WM_COMBOMOUSEACTIVATE, OnComboMouseActivate)

		CHAIN_MSG_MAP(CUpdateUI<CMainFrame>)

		COMMAND_ID_HANDLER(ID_APP_EXIT, OnFileExit)
		COMMAND_ID_HANDLER(ID_FILE_NEW, OnFileNew)
		COMMAND_ID_HANDLER(ID_FILE_OPEN, OnFileOpen)

		COMMAND_ID_HANDLER(ID_BROWSER_BACK, OnBrowserNavigate)
		COMMAND_ID_HANDLER(ID_BROWSER_FORWARD, OnBrowserNavigate)
		COMMAND_ID_HANDLER(ID_BROWSER_STOP, OnBrowserNavigate)
		COMMAND_ID_HANDLER(ID_BROWSER_REFRESH, OnBrowserNavigate)
		COMMAND_ID_HANDLER(ID_BROWSER_HOME, OnBrowserNavigate)
		COMMAND_ID_HANDLER(ID_BROWSER_SEARCH, OnBrowserNavigate)

		COMMAND_ID_HANDLER(ID_VIEW_TOOLBAR, OnViewToolBar)
		COMMAND_ID_HANDLER(ID_VIEW_ADDRESS_BAR, OnViewAddressBar)
		COMMAND_ID_HANDLER(ID_VIEW_LINKS_BAR, OnViewLinksBar)
		COMMAND_ID_HANDLER(ID_VIEW_STATUS_BAR, OnViewStatusBar)
		COMMAND_ID_HANDLER(ID_VIEW_LOCK_TOOLBARS, OnViewLockToolbars)

		COMMAND_ID_HANDLER(ID_APP_ABOUT, OnAppAbout)
		COMMAND_ID_HANDLER(ID_WINDOW_CLOSE, OnWindowClose)
		COMMAND_ID_HANDLER(ID_WINDOW_CLOSE_ALL, OnWindowCloseAll)
		COMMAND_ID_HANDLER(ID_NEXT_PANE, OnNextPrevPane)
		COMMAND_ID_HANDLER(ID_PREV_PANE, OnNextPrevPane)

		COMMAND_ID_HANDLER(ID_LINKS_MICROSOFT, OnLinks)
		COMMAND_ID_HANDLER(ID_LINKS_MSDN, OnLinks)
		COMMAND_ID_HANDLER(ID_LINKS_LIVE, OnLinks)
		COMMAND_ID_HANDLER(ID_LINKS_UPDATE, OnLinks)
		COMMAND_ID_HANDLER(ID_LINKS_WTL, OnLinks)

		COMMAND_RANGE_HANDLER(ID_WINDOW_TABFIRST, ID_WINDOW_TABLAST, OnWindowActivate)
		COMMAND_ID_HANDLER(ID_WINDOW_SHOWTABLIST, OnShowWindows)

		NOTIFY_CODE_HANDLER(TBVN_PAGEACTIVATED, OnTabViewPageActivated)
		NOTIFY_CODE_HANDLER(TBVN_CONTEXTMENU, OnTabViewContextMenu)

		NOTIFY_CODE_HANDLER(TBN_DROPDOWN, OnTBDropDown)
		COMMAND_ID_HANDLER(ID_GO, OnGo)

		CHAIN_MSG_MAP(CFrameWindowImpl<CMainFrame>)
	END_MSG_MAP()

	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	{
		// create command bar window
		HWND hWndCmdBar = m_CmdBar.Create(m_hWnd, rcDefault, NULL, ATL_SIMPLE_CMDBAR_PANE_STYLE);
		// attach menu
		m_CmdBar.AttachMenu(GetMenu());
		// load command bar images
		m_CmdBar.SetImageMaskColor(RGB(255, 0, 255));
		m_CmdBar.LoadImages(IDR_MAINFRAME);
		m_CmdBar.LoadImages(IDR_TABTOOLBAR);
		// remove old menu
		SetMenu(NULL);

		// create toolbar
		HWND hWndToolBar = CreateSimpleToolBarCtrl(m_hWnd, IDR_MAINFRAME_BIG, FALSE, ATL_SIMPLE_TOOLBAR_PANE_STYLE);

		// create rebar
		CreateSimpleReBar(ATL_SIMPLE_REBAR_NOBORDER_STYLE);
		AddSimpleReBarBand(hWndCmdBar);
		AddSimpleReBarBand(hWndToolBar, NULL, TRUE);

		// create address bar combo
		RECT rcCombo = { 0, 0, 1, 300 };
		m_cb.Create(m_hWnd, rcCombo, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | CBS_DROPDOWN | CBS_AUTOHSCROLL);

		CString strAddress;
		strAddress.LoadString(IDS_ADDRESS);
		AddSimpleReBarBand(m_cb, strAddress, FALSE, 10000);
		SizeSimpleReBarBands();

		// create links bar
		HWND hWndToolBarLinks = CreateSimpleToolBarCtrl(m_hWnd, IDB_PAGEIMAGE, FALSE, ATL_SIMPLE_TOOLBAR_PANE_STYLE | TBSTYLE_LIST);
		BuildLinksToolbar(hWndToolBarLinks);
		CString strLinks;
		strLinks.LoadString(IDS_LINKS);
		AddSimpleReBarBand(hWndToolBarLinks, strLinks);

		CReBarCtrl rebar = m_hWndToolBar;
		rebar.LockBands(true);

		CreateSimpleStatusBar();

		// create tab view
		m_hWndClient = m_view.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, WS_EX_CLIENTEDGE);
		m_view.SetTitleBarWindow(m_hWnd);

		// Set up update UI stuff
		UIAddToolBar(hWndToolBar);
		UIAddToolBar(m_cb.m_tb);

		UISetCheck(ID_VIEW_TOOLBAR, 1);
		UISetCheck(ID_VIEW_STATUS_BAR, 1);
		UISetCheck(ID_VIEW_ADDRESS_BAR, 1);
		UISetCheck(ID_VIEW_LINKS_BAR, 1);
		UISetCheck(ID_VIEW_LOCK_TOOLBARS, 1);

		UISetBlockAccelerators(true);

		// register object for message filtering and idle updates
		CMessageLoop* pLoop = _Module.GetMessageLoop();
		ATLASSERT(pLoop != NULL);
		pLoop->AddMessageFilter(this);
		pLoop->AddIdleHandler(this);

		// set tab view image list
		CImageList il;
		il.CreateFromImage(IDB_PAGEIMAGE, 16, 1, RGB(255, 0, 255), IMAGE_BITMAP, LR_CREATEDIBSECTION);
		m_view.SetImageList(il);

		// srt up window menu
		m_view.m_bWindowsMenuItem = true;
		CMenuHandle menuMain = m_CmdBar.GetMenu();
		m_view.SetWindowMenu(menuMain.GetSubMenu(WINDOW_MENU_POSITION));

		// get home page address
		bool bRet = GetHomePage();
		if(!bRet)
		{
			ATLTRACE(_T("TabBrowser: Can't get home page from the registry - using blank page\n"));
			m_strHomePage.LoadString(IDS_BLANK_URL);
		}

		return 0;
	}

	LRESULT OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
	{
		// unregister object for message filtering and idle updates
		CMessageLoop* pLoop = _Module.GetMessageLoop();
		ATLASSERT(pLoop != NULL);
		pLoop->RemoveMessageFilter(this);
		pLoop->RemoveIdleHandler(this);

		bHandled = FALSE;
		return 1;
	}

	LRESULT OnContextMenu(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
	{
		if((HWND)wParam == m_hWndToolBar || ::IsChild(m_hWndToolBar, (HWND)wParam))
		{
			POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
			CMenu menu;
			menu.LoadMenu(IDR_TOOLBAR_MENU);
			CMenuHandle menuPopup = menu.GetSubMenu(0);
			m_CmdBar.TrackPopupMenu(menuPopup, TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y);
		}
		else
		{
			bHandled = FALSE;
		}

		return 0;
	}

	LRESULT OnBrowserTitleChange(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
	{
		HWND hWnd = (HWND)wParam;
		LPCTSTR lpstrTitle = (LPCTSTR)lParam;

		CString strBlank;
		strBlank.LoadString(IDS_BLANK_URL);
		CString strBlankTitle;
		if(lstrcmp(lpstrTitle, strBlank) == 0)
		{
			strBlankTitle.LoadString(IDS_BLANK_TITLE);
			lpstrTitle = (LPCTSTR)strBlankTitle;
		}

		int nIndex = m_view.PageIndexFromHwnd(hWnd);
		m_view.SetPageTitle(nIndex, lpstrTitle);

		return 0;
	}

	LRESULT OnBrowserDocumentComplete(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
	{
		HWND hWndPage = (HWND)wParam;
		LPCTSTR lpstrAddress = (LPCTSTR)lParam;

		if(hWndPage == m_view.GetPageHWND(m_view.GetActivePage()))
			m_cb.SetWindowText(lpstrAddress);

		if(m_cb.FindStringExact(-1, lpstrAddress) == CB_ERR)
			m_cb.AddItem(lpstrAddress, -1, -1, 0);

		return 0;
	}

	LRESULT OnBrowserStatusTextChange(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/)
	{
		LPCTSTR lpstrText = (LPCTSTR)lParam;
		CStatusBarCtrl sb = m_hWndStatusBar;
		sb.SetText(ID_DEFAULT_PANE, lpstrText);

		return 0;
	}

	LRESULT OnComboMouseActivate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	{
		DeactivateHTMLPage();

		return 0;
	}

	LRESULT OnFileExit(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		PostMessage(WM_CLOSE);
		return 0;
	}

	LRESULT OnFileNew(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		OpenPage(m_strHomePage, true);

		return 0;
	}

	LRESULT OnFileOpen(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		CString strURL;

		int nActivePage = m_view.GetActivePage();
		if(nActivePage != -1)
		{
			CComPtr<IWebBrowser2> spWebBrowser = GetBrowser(nActivePage);
			ATLASSERT(spWebBrowser != NULL);
			BSTR bstrURL;
			spWebBrowser->get_LocationURL(&bstrURL);
			strURL = bstrURL;
		}

		COpenDlg dlg(strURL);
		if(dlg.DoModal() != IDOK)
			return 0;

		OpenPage(dlg.m_strURL, dlg.m_bNewTab);

		return 0;
	}

	LRESULT OnBrowserNavigate(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		int nActivePage = m_view.GetActivePage();
		if(nActivePage != -1)
		{
			CComPtr<IWebBrowser2> spWebBrowser = GetBrowser(nActivePage);
			ATLASSERT(spWebBrowser != NULL);
			HRESULT hRet = E_FAIL;
			switch(wID)
			{
			case ID_BROWSER_BACK:
				hRet = spWebBrowser->GoBack();
				break;
			case ID_BROWSER_FORWARD:
				hRet = spWebBrowser->GoForward();
				break;
			case ID_BROWSER_STOP:
				hRet = spWebBrowser->Stop();
				break;
			case ID_BROWSER_REFRESH:
				hRet = spWebBrowser->Refresh();
				break;
			case ID_BROWSER_HOME:
				hRet = spWebBrowser->GoHome();
				break;
			case ID_BROWSER_SEARCH:
				hRet = spWebBrowser->GoSearch();
				break;
			default:
				break;
			}
			ATLASSERT(SUCCEEDED(hRet));
		}
		else
		{
			::MessageBeep((UINT)-1);
		}

		return 0;
	}

	LRESULT OnViewToolBar(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		static BOOL bVisible = TRUE;	// initially visible
		bVisible = !bVisible;
		CReBarCtrl rebar = m_hWndToolBar;
		int nBandIndex = rebar.IdToIndex(ATL_IDW_BAND_FIRST + 1);	// toolbar is 2nd added band
		rebar.ShowBand(nBandIndex, bVisible);
		UISetCheck(ID_VIEW_TOOLBAR, bVisible);
		UpdateLayout();

		return 0;
	}

	LRESULT OnViewAddressBar(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		static BOOL bVisible = TRUE;	// initially visible
		bVisible = !bVisible;
		CReBarCtrl rebar = m_hWndToolBar;
		int nBandIndex = rebar.IdToIndex(ATL_IDW_BAND_FIRST + 2);	// address bar is 2nd added band
		rebar.ShowBand(nBandIndex, bVisible);
		UISetCheck(ID_VIEW_ADDRESS_BAR, bVisible);
		UpdateLayout();

		return 0;
	}

	LRESULT OnViewLinksBar(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		static BOOL bVisible = TRUE;	// initially visible
		bVisible = !bVisible;
		CReBarCtrl rebar = m_hWndToolBar;
		int nBandIndex = rebar.IdToIndex(ATL_IDW_BAND_FIRST + 3);	// links bar is 3rd added band
		rebar.ShowBand(nBandIndex, bVisible);
		UISetCheck(ID_VIEW_LINKS_BAR, bVisible);
		UpdateLayout();

		return 0;
	}

	LRESULT OnViewStatusBar(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		BOOL bVisible = !::IsWindowVisible(m_hWndStatusBar);
		::ShowWindow(m_hWndStatusBar, bVisible ? SW_SHOWNOACTIVATE : SW_HIDE);
		UISetCheck(ID_VIEW_STATUS_BAR, bVisible);
		UpdateLayout();

		return 0;
	}

	LRESULT OnViewLockToolbars(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		static bool bLock = true;	// initially locked
		bLock = !bLock;
		CReBarCtrl rebar = m_hWndToolBar;
		rebar.LockBands(bLock);
		UISetCheck(ID_VIEW_LOCK_TOOLBARS, bLock);

		return 0;
	}

	LRESULT OnAppAbout(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		CAboutDlg dlg;
		dlg.DoModal();

		return 0;
	}

	LRESULT OnWindowClose(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		int nActivePage = m_view.GetActivePage();
		if(nActivePage != -1)
			m_view.RemovePage(nActivePage);
		else
			::MessageBeep((UINT)-1);

		return 0;
	}

	LRESULT OnWindowCloseAll(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		m_view.RemoveAllPages();

		return 0;
	}

	LRESULT OnNextPrevPane(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		// we have only two destinations, so there is no need to look at the direction
		HWND hWndFocus = ::GetFocus();
		if(IsPageActive() && (hWndFocus == m_cb.m_hWnd || ::IsChild(m_cb.m_hWnd, hWndFocus)))
		{
			m_view.SetFocus();
		}
		else
		{
			DeactivateHTMLPage();
			m_cb.SetFocus();
		}

		return 0;
	}

	LRESULT OnLinks(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		ATLASSERT(wID >= ID_LINKS_FIRST && wID <= ID_LINKS_LAST);

		UINT nStringID = wID + (ID_LINKS_URL_FIRST - ID_LINKS_FIRST);
		CString strURL;
		strURL.LoadString(nStringID);
		ATLASSERT(!strURL.IsEmpty());

		OpenPage(strURL, !IsPageActive());

		return 0;
	}

	LRESULT OnWindowActivate(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		int nPage = wID - ID_WINDOW_TABFIRST;
		m_view.SetActivePage(nPage);

		return 0;
	}

	LRESULT OnShowWindows(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		CWindowsDlg dlg(&m_view);
		dlg.DoModal(m_hWnd);

		return 0;
	}

	LRESULT OnTabViewPageActivated(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
	{
		if(pnmh->idFrom != -1)
		{
			CComPtr<IWebBrowser2> spWebBrowser = GetBrowser(pnmh->idFrom);
			ATLASSERT(spWebBrowser != NULL);
			BSTR bstrURL;
			HRESULT hRet = spWebBrowser->get_LocationURL(&bstrURL);
			hRet;   // avoid level 4 warning
			ATLASSERT(SUCCEEDED(hRet));

			USES_CONVERSION;
			m_cb.SetWindowText(OLE2T(bstrURL));
		}
		else
		{
			m_cb.SetWindowText(_T(""));
		}

		return 0;
	}

	LRESULT OnTabViewContextMenu(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
	{
		LPTBVCONTEXTMENUINFO lpcmi = (LPTBVCONTEXTMENUINFO)pnmh;
		CMenuHandle menuMain = m_CmdBar.GetMenu();
		CMenuHandle menuPopup = menuMain.GetSubMenu(WINDOW_MENU_POSITION);	// Window sub menu
		int nRet = (int)m_CmdBar.TrackPopupMenu(menuPopup, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_RIGHTBUTTON, lpcmi->pt.x, lpcmi->pt.y);
		if(nRet == ID_WINDOW_CLOSE)
			m_view.RemovePage(pnmh->idFrom);
		else
			SendMessage(WM_COMMAND, MAKEWPARAM(nRet, 0));

		return 0;
	}

	LRESULT OnTBDropDown(int /*idCtrl*/, LPNMHDR pnmh, BOOL& bHandled)
	{
		LPNMTOOLBAR lpTB = (LPNMTOOLBAR)pnmh;

		if(lpTB->iItem != ID_WINDOW_SHOW_VIEWS)	// something else
		{
			bHandled = FALSE;
			return 1;
		}

		CToolBarCtrl wndToolBar = pnmh->hwndFrom;
		int nIndex = wndToolBar.CommandToIndex(ID_WINDOW_SHOW_VIEWS);
		RECT rect;
		wndToolBar.GetItemRect(nIndex, &rect);
		wndToolBar.ClientToScreen(&rect);

		CMenu menu;
		menu.CreatePopupMenu();

		m_view.BuildWindowMenu(menu);

		m_CmdBar.TrackPopupMenu(menu, TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_VERTICAL, rect.left, rect.bottom);

		return TBDDRET_DEFAULT;
	}

	LRESULT OnGo(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		CString str;
		int nLen = m_cb.GetWindowTextLength();
		m_cb.GetWindowText(str.GetBuffer(nLen), nLen + 1);
		str.ReleaseBuffer();

		str.TrimLeft();
		str.TrimRight();

		if(!str.IsEmpty())
			OpenPage(str, false);

		return 0;
	}
};
