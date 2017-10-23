// MainFrm.h : interface of the CMainFrame class
//
/////////////////////////////////////////////////////////////////////////////

#ifndef __MAINFRM_H__
#define __MAINFRM_H__

#pragma once

#define FILE_MENU_POSITION	0
#define RECENT_MENU_POSITION	6
#define POPUP_MENU_POSITION	0

LPCTSTR g_lpcstrMRURegKey = _T("Software\\Microsoft\\WTL Samples\\BmpView");
LPCTSTR g_lpcstrApp = _T("BmpView");


class CMainFrame : public CFrameWindowImpl<CMainFrame>, public CUpdateUI<CMainFrame>,
		public CMessageFilter, public CIdleHandler, public CPrintJobInfo
{
public:
	DECLARE_FRAME_WND_CLASS(NULL, IDR_MAINFRAME)

	CCommandBarCtrl m_CmdBar;
	CRecentDocumentList m_mru;
	CMruList m_list;
	CBitmapView m_view;

	TCHAR m_szFilePath[MAX_PATH];

	// printing support
	CPrinter m_printer;
	CDevMode m_devmode;
	CPrintPreviewWindow m_wndPreview;
	CEnhMetaFile m_enhmetafile;
	RECT m_rcMargin;
	bool m_bPrintPreview;


	CMainFrame() : m_bPrintPreview(false)
	{
		::SetRect(&m_rcMargin, 1000, 1000, 1000, 1000);
		m_printer.OpenDefaultPrinter();
		m_devmode.CopyFromPrinter(m_printer);
		m_szFilePath[0] = 0;
	}

	virtual BOOL PreTranslateMessage(MSG* pMsg)
	{
		if(CFrameWindowImpl<CMainFrame>::PreTranslateMessage(pMsg))
			return TRUE;

		return m_view.PreTranslateMessage(pMsg);
	}

	virtual BOOL OnIdle()
	{
		BOOL bEnable = !m_view.m_bmp.IsNull();
		UIEnable(ID_FILE_PRINT, bEnable);
		UIEnable(ID_FILE_PRINT_PREVIEW, bEnable);
		UISetCheck(ID_FILE_PRINT_PREVIEW, m_bPrintPreview);
		UIEnable(ID_EDIT_COPY, bEnable);
		UIEnable(ID_EDIT_PASTE, ::IsClipboardFormatAvailable(CF_BITMAP));
		UIEnable(ID_EDIT_CLEAR, bEnable);
		UIEnable(ID_VIEW_PROPERTIES, bEnable);
		UISetCheck(ID_RECENT_BTN, m_list.IsWindowVisible());
		UIUpdateToolBar();

		return FALSE;
	}

	void TogglePrintPreview()
	{
		if(m_bPrintPreview)	// close it
		{
			ATLASSERT(m_hWndClient == m_wndPreview.m_hWnd);

			m_hWndClient = m_view;
			m_view.ShowWindow(SW_SHOW);
			m_wndPreview.DestroyWindow();
		}
		else			// display it
		{
			ATLASSERT(m_hWndClient == m_view.m_hWnd);

			m_wndPreview.SetPrintPreviewInfo(m_printer, m_devmode.m_pDevMode, this, 0, 0);
			m_wndPreview.SetPage(0);

			m_wndPreview.Create(m_hWnd, rcDefault, NULL, 0, WS_EX_CLIENTEDGE);
			m_view.ShowWindow(SW_HIDE);
			m_hWndClient = m_wndPreview;
		}

		m_bPrintPreview = !m_bPrintPreview;
		UpdateLayout();
	}

	void UpdateTitleBar(LPCTSTR lpstrTitle)
	{
		CString strDefault;
		strDefault.LoadString(IDR_MAINFRAME);
		CString strTitle = strDefault;
		if(lpstrTitle != NULL)
		{
			strTitle = lpstrTitle;
			strTitle += _T(" - ");
			strTitle += strDefault;
		}
		SetWindowText(strTitle);
	}

	//print job info callbacks
	virtual bool IsValidPage(UINT nPage)
	{
		return (nPage == 0);	// we have only one page
	}

	virtual bool PrintPage(UINT nPage, HDC hDC)
	{
		if (nPage >= 1)		// we have only one page
			return false;

		ATLASSERT(!m_view.m_bmp.IsNull());

		RECT rcPage = 
			{ 0, 0, 
			::GetDeviceCaps(hDC, PHYSICALWIDTH) - 2 * ::GetDeviceCaps(hDC, PHYSICALOFFSETX),
			::GetDeviceCaps(hDC, PHYSICALHEIGHT) - 2 * ::GetDeviceCaps(hDC, PHYSICALOFFSETY) };

		CDCHandle dc = hDC;
		CClientDC dcScreen(m_hWnd);
		CDC dcMem;
		dcMem.CreateCompatibleDC(dcScreen);
		HBITMAP hBmpOld = dcMem.SelectBitmap(m_view.m_bmp);
		int cx = m_view.m_size.cx;
		int cy = m_view.m_size.cy;

		// calc scaling factor, so that bitmap is not too small
		// (based on the width only, max 3/4 width)
		int nScale = ::MulDiv(rcPage.right, 3, 4) / cx;
		if(nScale == 0)		// too big already
			nScale = 1;
		// calc margines to center bitmap
		int xOff = (rcPage.right - nScale * cx) / 2;
		if(xOff < 0)
			xOff = 0;
		int yOff = (rcPage.bottom - nScale * cy) / 2;
		if(yOff < 0)
			yOff = 0;
		// ensure that preview doesn't go outside of the page
		int cxBlt = nScale * cx;
		if(xOff + cxBlt > rcPage.right)
			cxBlt = rcPage.right - xOff;
		int cyBlt = nScale * cy;
		if(yOff + cyBlt > rcPage.bottom)
			cyBlt = rcPage.bottom - yOff;

		// now paint bitmap
		dc.StretchBlt(xOff, yOff, cxBlt, cyBlt, dcMem, 0, 0, cx, cy, SRCCOPY);

		dcMem.SelectBitmap(hBmpOld);

		return true;
	}

	BEGIN_MSG_MAP_EX(CMainFrame)
		MSG_WM_CREATE(OnCreate)
		MSG_WM_CONTEXTMENU(OnContextMenu)

		COMMAND_ID_HANDLER_EX(ID_FILE_OPEN, OnFileOpen)
		COMMAND_RANGE_HANDLER_EX(ID_FILE_MRU_FIRST, ID_FILE_MRU_LAST, OnFileRecent)
		COMMAND_ID_HANDLER_EX(ID_RECENT_BTN, OnRecentButton)
		COMMAND_ID_HANDLER_EX(ID_FILE_PRINT, OnFilePrint);
		COMMAND_ID_HANDLER_EX(ID_FILE_PAGE_SETUP, OnFilePageSetup)
		COMMAND_ID_HANDLER_EX(ID_FILE_PRINT_PREVIEW, OnFilePrintPreview);
		COMMAND_ID_HANDLER_EX(ID_APP_EXIT, OnFileExit)
		COMMAND_ID_HANDLER_EX(ID_EDIT_COPY, OnEditCopy)
		COMMAND_ID_HANDLER_EX(ID_EDIT_PASTE, OnEditPaste)
		COMMAND_ID_HANDLER_EX(ID_EDIT_CLEAR, OnEditClear)
		COMMAND_ID_HANDLER_EX(ID_VIEW_TOOLBAR, OnViewToolBar)
		COMMAND_ID_HANDLER_EX(ID_VIEW_STATUS_BAR, OnViewStatusBar)
		COMMAND_ID_HANDLER_EX(ID_VIEW_PROPERTIES, OnViewProperties)
		COMMAND_ID_HANDLER_EX(ID_APP_ABOUT, OnAppAbout)

		CHAIN_MSG_MAP(CUpdateUI<CMainFrame>)
		CHAIN_MSG_MAP(CFrameWindowImpl<CMainFrame>)
	END_MSG_MAP()

	BEGIN_UPDATE_UI_MAP(CMainFrame)
		UPDATE_ELEMENT(ID_FILE_PRINT, UPDUI_MENUPOPUP | UPDUI_TOOLBAR)
		UPDATE_ELEMENT(ID_FILE_PRINT_PREVIEW, UPDUI_MENUPOPUP | UPDUI_TOOLBAR)
		UPDATE_ELEMENT(ID_EDIT_COPY, UPDUI_MENUPOPUP | UPDUI_TOOLBAR)
		UPDATE_ELEMENT(ID_EDIT_PASTE, UPDUI_MENUPOPUP | UPDUI_TOOLBAR)
		UPDATE_ELEMENT(ID_EDIT_CLEAR, UPDUI_MENUPOPUP | UPDUI_TOOLBAR)
		UPDATE_ELEMENT(ID_VIEW_TOOLBAR, UPDUI_MENUPOPUP)
		UPDATE_ELEMENT(ID_VIEW_STATUS_BAR, UPDUI_MENUPOPUP)
		UPDATE_ELEMENT(ID_VIEW_PROPERTIES, UPDUI_MENUPOPUP | UPDUI_TOOLBAR)
		UPDATE_ELEMENT(ID_RECENT_BTN, UPDUI_TOOLBAR)
	END_UPDATE_UI_MAP()

	int OnCreate(LPCREATESTRUCT /*lpCreateStruct*/)
	{
		// create command bar window
		HWND hWndCmdBar = m_CmdBar.Create(m_hWnd, rcDefault, NULL, ATL_SIMPLE_CMDBAR_PANE_STYLE);
		// atach menu
		m_CmdBar.AttachMenu(GetMenu());
		// load command bar images
		m_CmdBar.LoadImages(IDR_MAINFRAME);
		// remove old menu
		SetMenu(NULL);

		// create toolbar and rebar
		HWND hWndToolBar = CreateSimpleToolBarCtrl(m_hWnd, IDR_MAINFRAME, FALSE, ATL_SIMPLE_TOOLBAR_PANE_STYLE);

		CreateSimpleReBar(ATL_SIMPLE_REBAR_NOBORDER_STYLE);
		AddSimpleReBarBand(hWndCmdBar);
		AddSimpleReBarBand(hWndToolBar, NULL, TRUE);

		// create status bar
		CreateSimpleStatusBar();

		// create view window
		m_hWndClient = m_view.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, WS_EX_CLIENTEDGE);
		m_view.SetBitmap(NULL);

		// set up MRU stuff
		CMenuHandle menu = m_CmdBar.GetMenu();
		CMenuHandle menuFile = menu.GetSubMenu(FILE_MENU_POSITION);
		CMenuHandle menuMru = menuFile.GetSubMenu(RECENT_MENU_POSITION);
		m_mru.SetMenuHandle(menuMru);
		m_mru.SetMaxEntries(12);

		m_mru.ReadFromRegistry(g_lpcstrMRURegKey);

		// create MRU list
		m_list.Create(m_hWnd);

		// set up update UI
		UIAddToolBar(hWndToolBar);
		UISetCheck(ID_VIEW_TOOLBAR, 1);
		UISetCheck(ID_VIEW_STATUS_BAR, 1);

		CMessageLoop* pLoop = _Module.GetMessageLoop();
		ATLASSERT(pLoop != NULL);
		pLoop->AddMessageFilter(this);
		pLoop->AddIdleHandler(this);

		return 0;
	}

	void OnContextMenu(CWindow wnd, CPoint point)
	{
		if(wnd.m_hWnd == m_view.m_hWnd)
		{
			CMenu menu;
			menu.LoadMenu(IDR_CONTEXTMENU);
			CMenuHandle menuPopup = menu.GetSubMenu(POPUP_MENU_POSITION);
			m_CmdBar.TrackPopupMenu(menuPopup, TPM_RIGHTBUTTON | TPM_VERTICAL, point.x, point.y);
		}
		else
		{
			SetMsgHandled(FALSE);
		}
	}

	void OnFileExit(UINT /*uNotifyCode*/, int /*nID*/, CWindow /*wnd*/)
	{
		PostMessage(WM_CLOSE);
	}

	void OnFileOpen(UINT /*uNotifyCode*/, int /*nID*/, CWindow /*wnd*/)
	{
		CFileDialog dlg(TRUE, _T("bmp"), NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, _T("Bitmap Files (*.bmp)\0*.bmp\0All Files (*.*)\0*.*\0"), m_hWnd);
		if(dlg.DoModal() == IDOK)
		{
			if(m_bPrintPreview)
				TogglePrintPreview();

			HBITMAP hBmp = (HBITMAP)::LoadImage(NULL, dlg.m_szFileName, IMAGE_BITMAP, 0, 0, LR_DEFAULTCOLOR | LR_LOADFROMFILE);
			if(hBmp != NULL)
			{
				m_view.SetBitmap(hBmp);
				m_mru.AddToList(dlg.m_ofn.lpstrFile);
				m_mru.WriteToRegistry(g_lpcstrMRURegKey);
				UpdateTitleBar(dlg.m_szFileTitle);
				lstrcpy(m_szFilePath, dlg.m_szFileName);
			}
			else
			{
				CString strMsg = _T("Can't load image from:\n");
				strMsg += dlg.m_szFileName;
				MessageBox(strMsg, g_lpcstrApp, MB_OK | MB_ICONERROR);
			}
		}
	}

	void OnFileRecent(UINT /*uNotifyCode*/, int nID, CWindow /*wnd*/)
	{
		if(m_bPrintPreview)
			TogglePrintPreview();

		// get file name from the MRU list
		TCHAR szFile[MAX_PATH];
		if(m_mru.GetFromList(nID, szFile, MAX_PATH))
		{
			// open file
			HBITMAP hBmp = (HBITMAP)::LoadImage(NULL, szFile, IMAGE_BITMAP, 0, 0, LR_DEFAULTCOLOR | LR_LOADFROMFILE);
			if(hBmp != NULL)
			{
				m_view.SetBitmap(hBmp);
				m_mru.MoveToTop(nID);
				// get file name without the path
				int nFileNamePos = 0;
				for(int i = lstrlen(szFile) - 1; i >= 0; i--)
				{
					if(szFile[i] == _T('\\'))
					{
						nFileNamePos = i + 1;
						break;
					}
				}
				UpdateTitleBar(&szFile[nFileNamePos]);
				lstrcpy(m_szFilePath, szFile);
			}
			else
			{
				m_mru.RemoveFromList(nID);
			}
			m_mru.WriteToRegistry(g_lpcstrMRURegKey);
		}
		else
		{
			::MessageBeep(MB_ICONERROR);
		}
	}

	void OnRecentButton(UINT /*uNotifyCode*/, int /*nID*/, CWindow /*wnd*/)
	{
		UINT uBandID = ATL_IDW_BAND_FIRST + 1;	// toolbar is second added band
		CReBarCtrl rebar = m_hWndToolBar;
		int nBandIndex = rebar.IdToIndex(uBandID);
		REBARBANDINFO rbbi = { 0 };
		rbbi.cbSize = RunTimeHelper::SizeOf_REBARBANDINFO();
		rbbi.fMask = RBBIM_CHILD;
		rebar.GetBandInfo(nBandIndex, &rbbi);
		CToolBarCtrl wndToolBar = rbbi.hwndChild;

		int nIndex = wndToolBar.CommandToIndex(ID_RECENT_BTN);
		CRect rect;
		wndToolBar.GetItemRect(nIndex, rect);
		wndToolBar.ClientToScreen(rect);

		// build and display MRU list in a popup
		m_list.BuildList(m_mru);
		m_list.ShowList(rect.left, rect.bottom);
	}

	void OnFilePrint(UINT /*uNotifyCode*/, int /*nID*/, CWindow /*wnd*/)
	{
		CPrintDialog dlg(FALSE);
		dlg.m_pd.hDevMode = m_devmode.CopyToHDEVMODE();
		dlg.m_pd.hDevNames = m_printer.CopyToHDEVNAMES();
		dlg.m_pd.nMinPage = 1;
		dlg.m_pd.nMaxPage = 1;

		if(dlg.DoModal() == IDOK)
		{
			m_devmode.CopyFromHDEVMODE(dlg.m_pd.hDevMode);
			m_printer.ClosePrinter();
			m_printer.OpenPrinter(dlg.m_pd.hDevNames, m_devmode.m_pDevMode);

			CPrintJob job;
			job.StartPrintJob(false, m_printer, m_devmode.m_pDevMode, this, _T("BmpView Document"), 0, 0, (dlg.PrintToFile() != FALSE));
		}

		::GlobalFree(dlg.m_pd.hDevMode);
		::GlobalFree(dlg.m_pd.hDevNames);
	}

	void OnFilePageSetup(UINT /*uNotifyCode*/, int /*nID*/, CWindow /*wnd*/)
	{
		CPageSetupDialog dlg;
		dlg.m_psd.hDevMode = m_devmode.CopyToHDEVMODE();
		dlg.m_psd.hDevNames = m_printer.CopyToHDEVNAMES();
		dlg.m_psd.rtMargin = m_rcMargin;

		if(dlg.DoModal() == IDOK)
		{
			if(m_bPrintPreview)
				TogglePrintPreview();

			m_devmode.CopyFromHDEVMODE(dlg.m_psd.hDevMode);
			m_printer.ClosePrinter();
			m_printer.OpenPrinter(dlg.m_psd.hDevNames, m_devmode.m_pDevMode);
			m_rcMargin = dlg.m_psd.rtMargin;
		}

		::GlobalFree(dlg.m_psd.hDevMode);
		::GlobalFree(dlg.m_psd.hDevNames);
	}

	void OnFilePrintPreview(UINT /*uNotifyCode*/, int /*nID*/, CWindow /*wnd*/)
	{
		TogglePrintPreview();
	}

	void OnEditCopy(UINT /*uNotifyCode*/, int /*nID*/, CWindow /*wnd*/)
	{
		if(::OpenClipboard(NULL))
		{
			HBITMAP hBitmapCopy = (HBITMAP)::CopyImage(m_view.m_bmp.m_hBitmap, IMAGE_BITMAP, 0, 0, 0);
			if(hBitmapCopy != NULL)
				::SetClipboardData(CF_BITMAP, hBitmapCopy);
			else
				MessageBox(_T("Can't copy bitmap"), g_lpcstrApp, MB_OK | MB_ICONERROR);

			::CloseClipboard();
		}
		else
		{
			MessageBox(_T("Can't open clipboard to copy"), g_lpcstrApp, MB_OK | MB_ICONERROR);
		}
	}

	void OnEditPaste(UINT /*uNotifyCode*/, int /*nID*/, CWindow /*wnd*/)
	{
		if(m_bPrintPreview)
			TogglePrintPreview();

		if(::OpenClipboard(NULL))
		{
			HBITMAP hBitmap = (HBITMAP)::GetClipboardData(CF_BITMAP);
			::CloseClipboard();
			if(hBitmap != NULL)
			{
				HBITMAP hBitmapCopy = (HBITMAP)::CopyImage(hBitmap, IMAGE_BITMAP, 0, 0, 0);
				if(hBitmapCopy != NULL)
				{
					m_view.SetBitmap(hBitmapCopy);
					UpdateTitleBar(_T("(Clipboard)"));
					m_szFilePath[0] = 0;
				}
				else
				{
					MessageBox(_T("Can't paste bitmap"), g_lpcstrApp, MB_OK | MB_ICONERROR);
				}
			}
			else
			{
				MessageBox(_T("Can't open bitmap from the clipboard"), g_lpcstrApp, MB_OK | MB_ICONERROR);
			}
		}
		else
		{
			MessageBox(_T("Can't open clipboard to paste"), g_lpcstrApp, MB_OK | MB_ICONERROR);
		}
	}

	void OnEditClear(UINT /*uNotifyCode*/, int /*nID*/, CWindow /*wnd*/)
	{
		if(m_bPrintPreview)
			TogglePrintPreview();

		m_view.SetBitmap(NULL);
		UpdateTitleBar(NULL);
		m_szFilePath[0] = 0;
	}

	void OnViewToolBar(UINT /*uNotifyCode*/, int /*nID*/, CWindow /*wnd*/)
	{
		static BOOL bNew = TRUE;	// initially visible
		bNew = !bNew;
		UINT uBandID = ATL_IDW_BAND_FIRST + 1;	// toolbar is second added band
		CReBarCtrl rebar = m_hWndToolBar;
		int nBandIndex = rebar.IdToIndex(uBandID);
		rebar.ShowBand(nBandIndex, bNew);
		UISetCheck(ID_VIEW_TOOLBAR, bNew);
		UpdateLayout();
	}

	void OnViewStatusBar(UINT /*uNotifyCode*/, int /*nID*/, CWindow /*wnd*/)
	{
		BOOL bNew = !::IsWindowVisible(m_hWndStatusBar);
		::ShowWindow(m_hWndStatusBar, bNew ? SW_SHOWNOACTIVATE : SW_HIDE);
		UISetCheck(ID_VIEW_STATUS_BAR, bNew);
		UpdateLayout();
	}

	void OnViewProperties(UINT /*uNotifyCode*/, int /*nID*/, CWindow /*wnd*/)
	{
		CBmpProperties prop;
		if(lstrlen(m_szFilePath) > 0)	// we have a file name
			prop.SetFileInfo(m_szFilePath, NULL);
		else				// must be clipboard then
			prop.SetFileInfo(NULL, m_view.m_bmp.m_hBitmap);
		prop.DoModal();
	}

	void OnAppAbout(UINT /*uNotifyCode*/, int /*nID*/, CWindow /*wnd*/)
	{
		CSimpleDialog<IDD_ABOUTBOX> dlg;
		dlg.DoModal();
	}
};

#endif // __MAINFRM_H__
