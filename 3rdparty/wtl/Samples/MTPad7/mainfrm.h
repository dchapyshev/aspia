#define FILE_MENU_POSITION	0
#define RECENT_MENU_POSITION	11

class CMainFrame : 
	public CRibbonFrameWindowImpl<CMainFrame>
	, public CPrintJobInfo
{
public:
	DECLARE_FRAME_WND_CLASS(NULL, IDR_MAINFRAME);

	CCommandBarCtrl m_CmdBar;
	CMultiPaneStatusBarCtrl m_sbar;
	CEditView m_view;
	BOOL m_bModified;

	CPrinterT<true> printer;
	CDevModeT<true> devmode;
	CPrintPreviewWindow prev;
	CEnhMetaFileT<true> enhmetafile;
	CRect m_rcMargin;
	HWND m_hWndOldClient;
	HWND m_hWndToolBarPP;
	CSimpleValArray<long> m_arrPages;

	CMainFrame() : m_bModified(FALSE), m_rcMargin(1000, 1000, 1000, 1000)
	{
		printer.OpenDefaultPrinter();
		devmode.CopyFromPrinter(printer);
	}

	// Ribbon Controls and map
	CRibbonRecentItemsCtrl<ID_RIBBON_RECENT_FILES> m_mru;
	CRibbonFontCtrl<ID_RIBBON_FONT> m_font;
	CRibbonSpinnerCtrl<ID_PAGE_SPINNER> m_spinner;
	CRibbonCommandCtrl<ID_GROUP_FONT> m_groupFont;
 
	BEGIN_RIBBON_CONTROL_MAP(CMainFrame)
		RIBBON_CONTROL(m_font)
		RIBBON_CONTROL(m_spinner)
		RIBBON_CONTROL(m_groupFont)
		RIBBON_CONTROL(m_mru)
	END_RIBBON_CONTROL_MAP()

	// Ribbon queries
	DWORD OnRibbonQueryFont(UINT /*nId*/, CHARFORMAT2& cf)
	{
		return m_view.GetDefaultCharFormat(cf);
	}

	bool OnRibbonQuerySpinnerValue(UINT /*nCmdID*/, REFPROPERTYKEY key, LONG* pVal)
	{
		if (key == UI_PKEY_DecimalValue)
		{
			*pVal = prev.m_nCurPage + 1;
			return true;
		}
		return false;
	}

	BOOL PreTranslateMessage(MSG* pMsg)
	{
		if(m_view.PreTranslateMessage(pMsg))
			return TRUE;

		return CFrameWindowImpl<CMainFrame>::PreTranslateMessage(pMsg);
	}

	BEGIN_MSG_MAP(CMainFrame)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		MESSAGE_HANDLER(WM_CLOSE, OnClose)
		MESSAGE_HANDLER(WM_UPDATEROWCOL, OnUpdateRowCol)
		COMMAND_ID_HANDLER(ID_FILE_PRINT, OnFilePrint)
		COMMAND_ID_HANDLER(ID_FILE_PRINT_PREVIEW, OnFilePrintPreview)
		COMMAND_ID_HANDLER(ID_FILE_PAGE_SETUP, OnFilePageSetup)
		COMMAND_ID_HANDLER(ID_PP_CLOSE, OnPrintPreviewClose)
		COMMAND_ID_HANDLER(ID_PP_BACK, OnPrintPreviewBack)
		COMMAND_ID_HANDLER(ID_PP_FORWARD, OnPrintPreviewForward)
		RIBBON_SPINNER_CONTROL_HANDLER(ID_PAGE_SPINNER, OnSpinnerPage)
		COMMAND_ID_HANDLER(ID_VIEW_RIBBON, OnViewRibbon)
		COMMAND_ID_HANDLER(ID_FILE_NEW, OnFileNew)
		COMMAND_ID_HANDLER(ID_FILE_OPEN, OnFileOpen)
		COMMAND_ID_HANDLER(ID_FILE_SAVE, OnFileSave)
		COMMAND_ID_HANDLER(ID_FILE_SAVE_AS, OnFileSaveAs)
		COMMAND_RANGE_HANDLER(ID_FILE_MRU_FIRST, ID_FILE_MRU_LAST, OnFileRecent)
		COMMAND_ID_HANDLER(ID_FILE_NEW_WINDOW, OnFileNewWindow)
		COMMAND_ID_HANDLER(ID_APP_EXIT, OnFileExit)
		COMMAND_ID_HANDLER(ID_VIEW_TOOLBAR, OnViewToolBar)
		COMMAND_ID_HANDLER(ID_VIEW_STATUS_BAR, OnViewStatusBar)
		COMMAND_ID_HANDLER(ID_APP_ABOUT, OnAppAbout)
		MESSAGE_HANDLER(WM_CONTEXTMENU, OnContextMenu)
		CHAIN_MSG_MAP(CRibbonFrameWindowImpl<CMainFrame>)
		CHAIN_COMMANDS_MEMBER((m_view))
	END_MSG_MAP()

	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	{
		// create command bar window
		HWND hWndCmdBar = m_CmdBar.Create(m_hWnd, rcDefault, NULL, ATL_SIMPLE_CMDBAR_PANE_STYLE);
		// atach menu
		m_CmdBar.AttachMenu(GetMenu());
		// load command bar images
		m_CmdBar.LoadImages(IDR_MAINFRAME);
		// remove old menu
		SetMenu(NULL);

		bool bRibbonUI = RunTimeHelper::IsRibbonUIAvailable();
		if (bRibbonUI) 
		{
			// UI Setup and adjustments
			UIAddMenu(m_CmdBar.GetMenu(), true);
			UIRemoveUpdateElement(ID_FILE_MRU_FIRST);
			UIPersistElement(ID_GROUP_PAGE);

			// Ribbon Controls initialization
			m_groupFont.SetImage(UI_PKEY_SmallImage, GetCommandBarBitmap(ID_FORMAT_FONT));
			m_spinner.SetValue(UI_PKEY_MinValue, 1);

			// Ribbon UI state and settings restoration
			CRibbonPersist(lpcstrMTPadRegKey).Restore(bRibbonUI, m_hgRibbonSettings);
		}
		else 
			CMenuHandle(m_CmdBar.GetMenu()).DeleteMenu(ID_VIEW_RIBBON, MF_BYCOMMAND); 

		HWND hWndToolBar = CreateSimpleToolBarCtrl(m_hWnd, IDR_MAINFRAME, FALSE, ATL_SIMPLE_TOOLBAR_PANE_STYLE);

		CreateSimpleReBar(ATL_SIMPLE_REBAR_NOBORDER_STYLE);
		AddSimpleReBarBand(hWndCmdBar);
		AddSimpleReBarBand(hWndToolBar, NULL, TRUE);

		CreateSimpleStatusBar();
		m_sbar.SubclassWindow(m_hWndStatusBar);
		int arrParts[] =
		{
			ID_DEFAULT_PANE,
			ID_ROW_PANE,
			ID_COL_PANE
		};
		m_sbar.SetPanes(arrParts, sizeof(arrParts) / sizeof(int), false);

		m_hWndClient = m_view.Create(m_hWnd, rcDefault, NULL, 
			WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | 
			ES_AUTOHSCROLL | ES_AUTOVSCROLL | ES_MULTILINE | ES_NOHIDESEL 
			| ES_SAVESEL | ES_SELECTIONBAR, WS_EX_CLIENTEDGE);

		UIAddToolBar(hWndToolBar);
		UISetCheck(ID_VIEW_TOOLBAR, 1);
		UISetCheck(ID_VIEW_STATUS_BAR, 1);

		SendMessage(WM_UPDATEROWCOL);	// update row and col indicators

		m_hWndToolBarPP = CreateSimpleToolBarCtrl(m_hWnd, IDR_PRINTPREVIEWBAR, FALSE, ATL_SIMPLE_TOOLBAR_PANE_STYLE, ATL_IDW_TOOLBAR + 1);
		AddSimpleReBarBand(m_hWndToolBarPP, NULL, TRUE);
		::SendMessage(m_hWndToolBar, RB_SHOWBAND, 2, FALSE);	// print preview toolbar is band #2

		UIAddToolBar(m_hWndToolBarPP);

		HMENU hMenu = m_CmdBar.GetMenu();
		HMENU hFileMenu = ::GetSubMenu(hMenu, FILE_MENU_POSITION);
#ifdef _DEBUG
		// absolute position, can change if menu changes
		TCHAR szMenuString[100];
		::GetMenuString(hFileMenu, RECENT_MENU_POSITION, szMenuString, sizeof(szMenuString), MF_BYPOSITION);
		ATLASSERT(lstrcmp(szMenuString, _T("Recent &Files")) == 0);
#endif //_DEBUG
		HMENU hMruMenu = ::GetSubMenu(hFileMenu, RECENT_MENU_POSITION);
		m_mru.SetMenuHandle(hMruMenu);

		m_mru.ReadFromRegistry(lpcstrMTPadRegKey);

		ShowRibbonUI(bRibbonUI); 
		UISetCheck(ID_VIEW_RIBBON, bRibbonUI);

		return 0;
	}

	LRESULT OnClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
	{
		if (RunTimeHelper::IsRibbonUIAvailable())
		{
			bool bRibbonUI = IsRibbonUI();
			if (bRibbonUI)
				SaveRibbonSettings();
			CRibbonPersist(lpcstrMTPadRegKey).Save(bRibbonUI, m_hgRibbonSettings);
		}
		bHandled = !m_view.QueryClose();
		return 0;
	}

	LRESULT OnUpdateRowCol(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	{
		TCHAR szBuff[100];
		wsprintf(szBuff, _T("Row: %i"), m_view.m_nRow + 1);
		m_sbar.SetPaneText(ID_ROW_PANE, szBuff);
		wsprintf(szBuff, _T("Col: %i"), m_view.m_nCol + 1);
		m_sbar.SetPaneText(ID_COL_PANE, szBuff);
		return 0;
	}

	LRESULT UpdateTitle()
	{
		int nAppendLen = lstrlen(m_view.m_strFileName);

		TCHAR strTitleBase[256];
		::LoadString(_Module.GetResourceInstance(), IDR_MAINFRAME, strTitleBase, 255);
		int nBaseLen = lstrlen(strTitleBase);

		// 1 == "*" (optional), 3 == " - ", 1 == null termination
		LPTSTR lpstrFullTitle = (LPTSTR)_alloca((nAppendLen + nBaseLen + 1 + 3 + 1) * sizeof(TCHAR));

		lstrcpy(lpstrFullTitle, m_view.m_strFileName);
		if(m_bModified)
			lstrcat(lpstrFullTitle, _T("*"));
		lstrcat(lpstrFullTitle, _T(" - "));
		lstrcat(lpstrFullTitle, strTitleBase);

		SetWindowText(lpstrFullTitle);

		return 0;
	}

	LRESULT OnFileNew(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		DoFileNew();
		return 0;
	}

	void DoFileNew()
	{
		if(m_view.QueryClose())
		{
			m_view.SetWindowText(NULL);
			m_view.Init(_T(""), _T("Untitled"));
			UpdateTitle();
		}
	}

	LRESULT OnFileOpen(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		CFileDialog dlg(TRUE, NULL, _T(""), OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, lpcstrFilter);
		int nRet = dlg.DoModal();

		if(nRet == IDOK)
		{
			ATLTRACE(_T("File path: %s\n"), dlg.m_ofn.lpstrFile);
			BOOL bRet = m_view.QueryClose();
			if(!bRet)
			{
				if(!DoFileSaveAs())
					return 0;
			}

			if(DoFileOpen(dlg.m_ofn.lpstrFile, dlg.m_ofn.lpstrFileTitle))
			{
				m_mru.AddToList(dlg.m_ofn.lpstrFile);
				m_mru.WriteToRegistry(lpcstrMTPadRegKey);
			}
		}

		return 0;
	}

	BOOL DoFileOpen(LPCTSTR lpstrFileName, LPCTSTR lpstrFileTitle)
	{
		BOOL bRet = m_view.LoadFile(lpstrFileName);
		if(bRet)
		{
			m_view.Init(lpstrFileName, lpstrFileTitle);
			UpdateTitle();
		}
		else
			MessageBox(_T("Error reading file!\n"));

		return bRet;
	}

	LRESULT OnFileSave(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		if(m_view.m_strFilePath[0] != 0)
		{
			if(m_view.SaveFile(m_view.m_strFilePath))
			{
				m_view.SetModify(FALSE);
				m_mru.AddToList(m_view.m_strFilePath);
				m_mru.WriteToRegistry(lpcstrMTPadRegKey);
			}
			else
			{
				MessageBox(_T("Error writing file!\n"));
			}
		}
		else
		{
			if(m_view.DoFileSaveAs())
			{
				UpdateTitle();
				m_mru.AddToList(m_view.m_strFilePath);
				m_mru.WriteToRegistry(lpcstrMTPadRegKey);
			}
		}

		return 0;
	}

	LRESULT OnFileSaveAs(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		if(m_view.DoFileSaveAs())
		{
			UpdateTitle();
			m_mru.AddToList(m_view.m_strFilePath);
			m_mru.WriteToRegistry(lpcstrMTPadRegKey);
		}
		return 0;
	}

	BOOL DoFileSaveAs()
	{
		BOOL bRet = FALSE;
		CFileDialog dlg(FALSE);
		int nRet = dlg.DoModal();

		if(nRet == IDOK)
		{
			ATLTRACE(_T("File path: %s\n"), dlg.m_ofn.lpstrFile);
			bRet = m_view.SaveFile(dlg.m_ofn.lpstrFile);
			if(bRet)
			{
				m_view.Init(dlg.m_ofn.lpstrFile, dlg.m_ofn.lpstrFileTitle);
				UpdateTitle();
			}
			else
				MessageBox(_T("Error writing file!\n"));
		}

		return bRet;
	}

	LRESULT OnFileRecent(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		// check if we have to save the current one
		if(!m_view.QueryClose())
		{
			if(!m_view.DoFileSaveAs())
				return 0;
		}
		// get file name from the MRU list
		TCHAR szFile[MAX_PATH];
		if(m_mru.GetFromList(wID, szFile, MAX_PATH))
		{
			// find file name without the path
			LPTSTR lpstrFileName = szFile;
			for(int i = lstrlen(szFile) - 1; i >= 0; i--)
			{
				if(szFile[i] == '\\')
				{
					lpstrFileName = &szFile[i + 1];
					break;
				}
			}
			// open file
			if(DoFileOpen(szFile, lpstrFileName))
				m_mru.MoveToTop(wID);
			else
				m_mru.RemoveFromList(wID);
			m_mru.WriteToRegistry(lpcstrMTPadRegKey);
		}
		else
		{
			::MessageBeep(MB_ICONERROR);
		}

		return 0;
	}

	LRESULT OnFileNewWindow(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		::PostThreadMessage(_Module.m_dwMainThreadID, WM_USER, 0, 0L);
		return 0;
	}

	LRESULT OnFileExit(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		PostMessage(WM_CLOSE);
		return 0;
	}

	LRESULT OnViewToolBar(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		static BOOL bNew = TRUE;	// initially visible
		bNew = !bNew;
		::SendMessage(m_hWndToolBar, RB_SHOWBAND, 1, bNew); // toolbar is band #1
		UISetCheck(ID_VIEW_TOOLBAR, bNew);
		UpdateLayout();
		return 0;
	}

	LRESULT OnViewStatusBar(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		BOOL bNew = !::IsWindowVisible(m_hWndStatusBar);
		::ShowWindow(m_hWndStatusBar, bNew ? SW_SHOWNOACTIVATE : SW_HIDE);
		UISetCheck(ID_VIEW_STATUS_BAR, bNew);
		UpdateLayout();
		return 0;
	}

	LRESULT OnAppAbout(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		CAboutDlg dlg;
		dlg.DoModal();
		return 0;
	}

	LRESULT OnViewRibbon(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) 
	{
		ShowRibbonUI(!IsRibbonUI(), UI_MAKEAPPMODE(prev.IsWindow())); 
		UISetCheck(ID_VIEW_RIBBON, IsRibbonUI());
		return 0; 
	} 

	LRESULT OnContextMenu(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
	{
		if((HWND)wParam == m_view.m_hWnd)
		{
			if (IsRibbonUI())
				TrackRibbonMenu(ID_CONTEXTMAP, lParam);
			else
			{
				CMenu menuContext;
				menuContext.LoadMenu(IDR_CONTEXTMENU);
				CMenuHandle menuPopup(menuContext.GetSubMenu(0));
					m_CmdBar.TrackPopupMenu(menuPopup, TPM_LEFTALIGN | TPM_RIGHTBUTTON, LOWORD(lParam), HIWORD(lParam));
			}
		}
		else
		{
			bHandled = FALSE;
		}

		return 0;
	}

	void UpdateUIAll()
	{
		BOOL bModified = m_view.GetModify();
		if(bModified != m_bModified)
		{
			m_bModified = bModified;
			UpdateTitle();
		}

		UIEnable(ID_EDIT_UNDO, m_view.CanUndo());
		UIEnable(ID_EDIT_PASTE, m_view.CanPaste(CF_TEXT));

		BOOL bSel = (m_view.GetSelectionType() != SEL_EMPTY);

		UIEnable(ID_EDIT_CUT, bSel);
		UIEnable(ID_EDIT_COPY, bSel);
		UIEnable(ID_EDIT_CLEAR, bSel);

		BOOL bNotEmpty = (m_view.GetWindowTextLength() > 0);
		UIEnable(ID_EDIT_SELECT_ALL, bNotEmpty);
		UIEnable(ID_EDIT_FIND, bNotEmpty);
		UIEnable(ID_EDIT_REPEAT, bNotEmpty);
		UIEnable(ID_EDIT_REPLACE, bNotEmpty);

		UISetCheck(ID_EDIT_WORD_WRAP, m_view.m_bWordWrap);

		if(prev.IsWindow() && (prev.GetStyle() & WS_VISIBLE) != 0)
		{
			UIEnable(ID_PP_BACK, (prev.m_nCurPage > prev.m_nMinPage));
			UIEnable(ID_PP_FORWARD, prev.m_nCurPage < prev.m_nMaxPage);
		}

		UIUpdateToolBar();
	}

//print job info callback
	virtual bool IsValidPage(UINT /*nPage*/)
	{
		return true;
	}

	virtual bool PrintPage(UINT nPage, HDC hDC)
	{
		if (nPage >= (UINT)m_arrPages.GetSize())
			return false;

		RECT rcPage;
		rcPage.left = rcPage.top = 0;
		rcPage.bottom = GetDeviceCaps(hDC, PHYSICALHEIGHT);
		rcPage.right = GetDeviceCaps(hDC, PHYSICALWIDTH);

		rcPage.right = MulDiv(rcPage.right, 1440, GetDeviceCaps(hDC, LOGPIXELSX));
		rcPage.bottom = MulDiv(rcPage.bottom, 1440, GetDeviceCaps(hDC, LOGPIXELSY));

		RECT rcOutput = rcPage;
		//convert from 1/1000" to twips
		rcOutput.left += MulDiv(m_rcMargin.left, 1440, 1000);
		rcOutput.right -= MulDiv(m_rcMargin.right, 1440, 1000);
		rcOutput.top += MulDiv(m_rcMargin.top, 1440, 1000);
		rcOutput.bottom -= MulDiv(m_rcMargin.bottom, 1440, 1000);

		
		FORMATRANGE fr;
		fr.hdc = hDC;
		fr.hdcTarget = hDC;
		fr.rc = rcOutput;
		fr.rcPage = rcPage;
		fr.chrg.cpMin = 0;
		fr.chrg.cpMax = -1;
		fr.chrg.cpMin = m_arrPages[nPage];

		// We have to adjust the origin because 0,0 is not at the corner of the paper
		// but is at the corner of the printable region
		int nOffsetX = GetDeviceCaps(hDC, PHYSICALOFFSETX);
		int nOffsetY = GetDeviceCaps(hDC, PHYSICALOFFSETY);
		SetViewportOrgEx(hDC, -nOffsetX, -nOffsetY, NULL);
		m_view.FormatRange(fr, TRUE);
		m_view.DisplayBand(&rcOutput);

		//Cleanup cache in richedit
		m_view.FormatRange(NULL, FALSE);
		return true;
	}

	bool LayoutPages()
	{
		CDC dc = printer.CreatePrinterDC(devmode);
		if(dc.IsNull())
			return false;

		RECT rcPage;
		rcPage.left = rcPage.top = 0;
		rcPage.bottom = GetDeviceCaps(dc, PHYSICALHEIGHT);
		rcPage.right = GetDeviceCaps(dc, PHYSICALWIDTH);
		// We have to adjust the origin because 0,0 is not at the corner of the paper
		// but is at the corner of the printable region
		int nOffsetX = dc.GetDeviceCaps(PHYSICALOFFSETX);
		int nOffsetY = dc.GetDeviceCaps(PHYSICALOFFSETY);
		dc.SetViewportOrg(-nOffsetX, -nOffsetY);
		rcPage.right = MulDiv(rcPage.right, 1440, GetDeviceCaps(dc, LOGPIXELSX));
		rcPage.bottom = MulDiv(rcPage.bottom, 1440, GetDeviceCaps(dc, LOGPIXELSY));

		RECT rcOutput = rcPage;
		//convert from 1/1000" to twips
		rcOutput.left += MulDiv(m_rcMargin.left, 1440, 1000);
		rcOutput.right -= MulDiv(m_rcMargin.right, 1440, 1000);
		rcOutput.top += MulDiv(m_rcMargin.top, 1440, 1000);
		rcOutput.bottom -= MulDiv(m_rcMargin.bottom, 1440, 1000);
		
		FORMATRANGE fr;
		fr.hdc = dc;
		fr.hdcTarget = dc;
		fr.rc = rcOutput;
		fr.rcPage = rcPage;
		fr.chrg.cpMin = 0;
		fr.chrg.cpMax = -1;

		LONG n = (LONG)m_view.GetTextLength();
		m_arrPages.RemoveAll();
		while (1)
		{
			m_arrPages.Add(fr.chrg.cpMin);
			LONG lRet = m_view.FormatRange(fr, FALSE);
			if((lRet - fr.chrg.cpMin) == -1)
			{
				m_arrPages.RemoveAt(m_arrPages.GetSize()-1);
				break;
			}
			else
				fr.chrg.cpMin = lRet;
			if (fr.chrg.cpMin >= n)
				break;
		}

		m_view.FormatRange(NULL, FALSE);

		return true;
	}

	LRESULT OnFilePrint(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		if(!LayoutPages())
		{
			MessageBox(_T("Print operation failed"), _T("MTPad"), MB_ICONERROR | MB_OK);
			return 0;
		}

		CPrintDialog dlg(FALSE);
		dlg.m_pd.hDevMode = devmode.CopyToHDEVMODE();
		dlg.m_pd.hDevNames = printer.CopyToHDEVNAMES();
		dlg.m_pd.nMinPage = 1;
		dlg.m_pd.nMaxPage = (WORD)m_arrPages.GetSize();
		dlg.m_pd.nFromPage = 1;
		dlg.m_pd.nToPage = (WORD)m_arrPages.GetSize();
		dlg.m_pd.Flags &= ~PD_NOPAGENUMS;

		if (dlg.DoModal() == IDOK)
		{
			devmode.CopyFromHDEVMODE(dlg.m_pd.hDevMode);
			printer.ClosePrinter();
			printer.OpenPrinter(dlg.m_pd.hDevNames, devmode.m_pDevMode);

			CPrintJob job;
			int nMin=0;
			int nMax = m_arrPages.GetSize() - 1;
			if(dlg.PrintRange() != FALSE)
			{
				nMin = dlg.m_pd.nFromPage - 1;
				nMax = dlg.m_pd.nToPage - 1;
			}

			job.StartPrintJob(false, printer, devmode.m_pDevMode, this, _T("MTPad Document"), nMin, nMax, (dlg.PrintToFile() != FALSE));
		}

		GlobalFree(dlg.m_pd.hDevMode);
		GlobalFree(dlg.m_pd.hDevNames);

		return 0;
	}

LRESULT OnFilePrintPreview(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
		bool bRet = LayoutPages();
		if(!bRet)
		{
			MessageBox(_T("Print preview operation failed"), _T("MTPad"), MB_ICONERROR | MB_OK);
			return 0;
		}

		prev.SetPrintPreviewInfo(printer, devmode.m_pDevMode, this, 0, m_arrPages.GetSize() - 1);
		prev.SetPage(0);
		RECT rcPos;
		GetClientRect(&rcPos);
		prev.Create(m_hWnd, rcPos);
		prev.ShowWindow(SW_SHOW);
		prev.SetWindowPos(HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE|SWP_NOSIZE|SWP_SHOWWINDOW);

		::ShowWindow(m_hWndClient, SW_HIDE);
		m_hWndOldClient = m_hWndClient;
		m_hWndClient = prev;
		::SendMessage(m_hWndToolBar, RB_SHOWBAND, 1, FALSE);	// toolbar is band #1
		::SendMessage(m_hWndToolBar, RB_SHOWBAND, 2, TRUE); // print preview toolbar is band #2
		UpdateLayout();


		m_spinner.SetValue(UI_PKEY_MaxValue, m_arrPages.GetSize(), true);

		CString sPageGroup = L"1 Page";
		INT nPage = m_arrPages.GetSize();
		if (nPage > 1)
			sPageGroup.Format(L"%d Pages", nPage);
		UISetText(ID_GROUP_PAGE, sPageGroup);

		if (IsRibbonUI())
		{
			SetRibbonModes(UI_MAKEAPPMODE(1));
		}
		return 0;
	}

	LRESULT OnFilePageSetup(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		CPageSetupDialog dlg;
		dlg.m_psd.hDevMode = devmode.CopyToHDEVMODE();
		dlg.m_psd.hDevNames = printer.CopyToHDEVNAMES();
		dlg.m_psd.rtMargin = m_rcMargin;

		if (dlg.DoModal() == IDOK)
		{
			devmode.CopyFromHDEVMODE(dlg.m_psd.hDevMode);
			printer.ClosePrinter();
			printer.OpenPrinter(dlg.m_psd.hDevNames, devmode.m_pDevMode);
			m_rcMargin = dlg.m_psd.rtMargin;
		}

		GlobalFree(dlg.m_psd.hDevMode);
		GlobalFree(dlg.m_psd.hDevNames);

		return 0;
	}

	LRESULT OnPrintPreviewClose(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		if (IsRibbonUI())
			SetRibbonModes(UI_MAKEAPPMODE(0));

		prev.DestroyWindow();
		m_hWndClient = m_hWndOldClient;
		::ShowWindow(m_hWndClient, SW_SHOW);
		::SendMessage(m_hWndToolBar, RB_SHOWBAND, 1, TRUE); // toolbar is band #1
		::SendMessage(m_hWndToolBar, RB_SHOWBAND, 2, FALSE);	// print preview toolbar is band #2
		UpdateLayout();
		return 0;
	}

	LRESULT OnPrintPreviewForward(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		prev.NextPage();
		m_spinner.SetVal(prev.m_nCurPage + 1, true);
		return 0;
	}
	
	LRESULT OnPrintPreviewBack(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		prev.PrevPage();
		m_spinner.SetVal(prev.m_nCurPage + 1, true);
		return 0;
	}

	LRESULT OnSpinnerPage(WORD /*wID*/, LONG lVal, BOOL& /*bHandled*/)
	{
		prev.SetPage(lVal - 1);
		prev.Invalidate();
		return 0;
	}

};
