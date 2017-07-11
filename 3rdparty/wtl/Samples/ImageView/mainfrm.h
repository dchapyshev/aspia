// MainFrm.h : interface of the CMainFrame class
//
/////////////////////////////////////////////////////////////////////////////

#pragma once

#if !defined(_IMAGEVIEW_MAINFRM_H_)
#define _IMAGEVIEW_MAINFRM_H_


// Selective message forwarding macros

#define FORWARD_MSG(msg) if ( uMsg == msg ) \
	{ lResult = ::SendMessage( GetParent(), uMsg, wParam, lParam ); return bHandled = TRUE;}

#define FORWARD_NOTIFICATION_ID(uID) if (( uMsg == WM_NOTIFY) && ( wParam == uID)) \
	{ lResult = ::SendMessage( GetParent(), uMsg, wParam, lParam ); return bHandled = TRUE;}


/////////////////////
// CMainFrame : Application main window

typedef CWinTraits< WS_CLIPCHILDREN | WS_CLIPSIBLINGS ,0> CCeFrameTraits;

class CMainFrame : 
		public CFrameWindowImpl<CMainFrame,CWindow,CCeFrameTraits>, 
		public CUpdateUI<CMainFrame>,
		public CMessageFilter, public CIdleHandler, 
		public CFullScreenFrame<CMainFrame, false>,
		public CAppWindow<CMainFrame>
{

// CZoomMenuBar: MenuBar forwarding trackbar messages
	class CZoomMenuBar : public CWindowImpl< CZoomMenuBar, CMenuBarCtrl >
	{
	public:
		DECLARE_WND_SUPERCLASS( L"ZoomMenuBar", L"ToolbarWindow32");

		BEGIN_MSG_MAP(CZoomMenuBar)
			FORWARD_MSG(WM_HSCROLL)
			FORWARD_MSG(WM_CTLCOLORSTATIC)
			FORWARD_NOTIFICATION_ID(ID_ZOOM)
		END_MSG_MAP()
	};

// Data and declarations
public:
	typedef CFrameWindowImpl<CMainFrame,CWindow,CCeFrameTraits> BaseFrame;
	typedef CFullScreenFrame<CMainFrame, false> BaseFullScreen;

	DECLARE_APP_FRAME_CLASS(NULL, IDR_MAINFRAME, L"Software\\WTL\\ImageView")
	CImageViewView m_view;
	CString m_sFile;
	CZoomMenuBar m_MenuBar;

// CMessageFilter pure virtual definition
	virtual BOOL PreTranslateMessage(MSG* pMsg)
	{
		if( BaseFrame::PreTranslateMessage(pMsg))
			return TRUE;

		return m_view.PreTranslateMessage(pMsg);
	}


// CFrameWindowImplBase::UpdateLayout override for CCS_TOP status bar
	void UpdateLayout(BOOL bResizeBars = TRUE)
	{
		CRect rectWnd, rectTool;
		GetClientRect( rectWnd);

		// resize title bar
		if( m_view.m_TitleBar.IsWindow() && ( m_view.m_TitleBar.GetStyle() & WS_VISIBLE))
		{
			if(bResizeBars)
				m_view.m_TitleBar.SendMessage( WM_SIZE);
			m_view.m_TitleBar.GetWindowRect( rectTool);
			rectWnd.top += rectTool.Size().cy;
		}

		// resize view
		if ( m_view.IsWindow())
		{
			m_view.GetWindowRect( rectTool);
			if ( rectTool != rectWnd)
				m_view.SetWindowPos( NULL, rectWnd, SWP_NOZORDER | SWP_NOACTIVATE);
		}
	}

// CAppWindow operations
	bool AppHibernate( bool bHibernate)
	{
		if ( bHibernate)	// go to sleep
			if ( m_sFile.IsEmpty()) // clipboard or no image
				return false;
			else
				m_view.m_bmp.DeleteObject();
		
		else	// wake up 
			if ( HBITMAP hbm = LoadImageFile( m_sFile))
				m_view.m_bmp.Attach( hbm);
			else	// file was moved or deleted during hibernation
				CloseImage();

		return bHibernate;
	}

	bool AppNewInstance( LPCTSTR lpstrCmdLine)
	{
		return SetImageFile( lpstrCmdLine) != NULL;
	}

	void AppSave()
	{
		CAppInfo info;
		BOOL bTitle = m_view.m_TitleBar.IsWindowVisible();
		info.Save( bTitle, L"TitleBar");
		info.Save( m_view.m_bShowScroll, L"ScrollBars");
		info.Save( m_bFullScreen, L"Full");
		info.Save( m_sFile, L"Image");
		info.Save( m_view.GetScrollOffset(), L"Scroll");
		info.Save( m_view.m_fzoom, L"Zoom");
	}

// File and image operations
	HBITMAP LoadImageFile( LPCTSTR szFileName )
	{
		CString szImage = szFileName;
		CBitmapHandle hBmp = szImage.Find( L".bmp") != -1 ? 
			::SHLoadDIBitmap( szFileName) : ::SHLoadImageFile( szFileName);

		if( hBmp.IsNull())
		{
			CString strMsg = L"Cannot load image from: ";
			strMsg += szFileName;
			AtlMessageBox( m_hWnd, (LPCTSTR)strMsg, IDR_MAINFRAME, MB_OK | MB_ICONERROR);
		}
		return hBmp;
	}
	
	bool SetImageFile( LPCTSTR szFileName, double fZoom = 1.0 , POINT ptScroll= CPoint( -1, -1))
	{
		CBitmapHandle hBmp;
		if ( szFileName && *szFileName)
			hBmp = LoadImageFile( szFileName);
		bool bOK = !hBmp.IsNull();
		if( bOK)
		{
			m_sFile = szFileName;
			CString sImageName = m_sFile.Mid( m_sFile.ReverseFind(L'\\') + 1);
			m_view.SetImage( hBmp, sImageName.Left( sImageName.ReverseFind(L'.')), fZoom, ptScroll );
		}
		else
			CloseImage();

		UIEnable(ID_ZOOM, bOK);
		UIEnable(ID_FILE_CLOSE, bOK);
		UIEnable(ID_EDIT_COPY, bOK);
		UIEnable(ID_VIEW_PROPERTIES, bOK);
		return bOK;
	}

	void CloseImage()
	{
		m_sFile.Empty();
		m_view.SetImage( NULL);
	}

// UpdateUI operations and map
	virtual BOOL OnIdle()
	{
		UIEnable( ID_EDIT_PASTE, IsClipboardFormatAvailable( CF_DIB));
		UIUpdateToolBar();
		UIUpdateChildWindows();
		return FALSE;
	}

	BEGIN_UPDATE_UI_MAP(CMainFrame)
		UPDATE_ELEMENT(ID_EDIT_COPY, UPDUI_MENUPOPUP | UPDUI_TOOLBAR)
		UPDATE_ELEMENT(ID_EDIT_PASTE, UPDUI_MENUPOPUP | UPDUI_TOOLBAR)
		UPDATE_ELEMENT(ID_FILE_CLOSE, UPDUI_MENUPOPUP | UPDUI_TOOLBAR)
		UPDATE_ELEMENT(ID_VIEW_PROPERTIES, UPDUI_MENUPOPUP | UPDUI_TOOLBAR)
		UPDATE_ELEMENT(ID_VIEW_TOOLBAR, UPDUI_MENUPOPUP)
		UPDATE_ELEMENT(ID_VIEW_SCROLLBARS, UPDUI_MENUPOPUP)
		UPDATE_ELEMENT(ID_VIEW_STATUS_BAR, UPDUI_MENUPOPUP)
		UPDATE_ELEMENT(ID_ZOOM, UPDUI_CHILDWINDOW)
	END_UPDATE_UI_MAP()

// Message map and handlers
	BEGIN_MSG_MAP(CMainFrame)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		NOTIFY_CODE_HANDLER(GN_CONTEXTMENU, OnContextMenu)
		COMMAND_ID_HANDLER(ID_APP_EXIT, OnExit)
		COMMAND_ID_HANDLER(ID_FILE_OPEN, OnFileOpen)
		COMMAND_ID_HANDLER(ID_FILE_CLOSE, OnFileClose)
		COMMAND_ID_HANDLER(ID_EDIT_PASTE, OnPaste)
		COMMAND_ID_HANDLER(ID_APP_ABOUT, OnAppAbout)
		COMMAND_ID_HANDLER(ID_FILE_REGISTER, OnRegister)
		COMMAND_ID_HANDLER(ID_VIEW_PROPERTIES, OnProperties)
		COMMAND_ID_HANDLER(ID_VIEW_STATUS_BAR, OnTitle)
		COMMAND_ID_HANDLER(ID_VIEW_TOOLBAR, OnFullScreen)
		COMMAND_ID_HANDLER(ID_VIEW_SCROLLBARS, OnScrollBars)
		CHAIN_MSG_MAP(BaseFullScreen)
		CHAIN_MSG_MAP(CAppWindow<CMainFrame>)
		CHAIN_MSG_MAP(CUpdateUI<CMainFrame>)
		CHAIN_MSG_MAP(BaseFrame)
		CHAIN_MSG_MAP_ALT_MEMBER(m_view, 1)
	END_MSG_MAP()

// Creation and destruction
	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/)
	{
		CAppInfo info;

		// Full screen delayed restoration 
		bool bFull = false;
		info.Restore( bFull, L"Full");
		if ( bFull)
			PostMessage( WM_COMMAND, ID_VIEW_TOOLBAR);

		// MenuBar creation
		CreateSimpleCEMenuBar( IDR_MAINFRAME, SHCMBF_HIDESIPBUTTON);
		m_MenuBar.SubclassWindow( m_hWndCECommandBar);
		m_MenuBar.LoadStdImages( IDB_STD_SMALL_COLOR);
		UIAddToolBar( m_hWndCECommandBar);

		// Trackbar creation
		CRect rZoom;
		m_MenuBar.GetRect( ID_ZOOM, rZoom);
		rZoom.top -= 1;
		m_view.m_ZoomCtrl.Create( m_hWndCECommandBar, rZoom, NULL ,WS_CHILD | TBS_TOP | TBS_FIXEDLENGTH, 0, 
			ID_ZOOM );
		m_view.m_ZoomCtrl.SetThumbLength( 9); 
		rZoom.DeflateRect( 1, 1);
		m_view.m_ZoomCtrl.SetWindowPos( HWND_TOP, rZoom.left, rZoom.top + 1, 
			rZoom.Width(), rZoom.Height(), SWP_SHOWWINDOW);	
		UIAddChildWindowContainer( m_hWndCECommandBar);

		// TitleBar creation
		BOOL bTitle = TRUE;
		info.Restore( bTitle, L"TitleBar");
		DWORD dwStyle = WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | CCS_TOP;
		if ( bTitle)
			dwStyle |= WS_VISIBLE;
		CreateSimpleStatusBar( L"", dwStyle);
		m_view.m_TitleBar.Attach( m_hWndStatusBar);
		UISetCheck( ID_VIEW_STATUS_BAR, bTitle);

		// View creation
		info.Restore( m_view.m_bShowScroll, L"ScrollBars");
		m_hWndClient = m_view.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);
		UISetCheck( ID_VIEW_SCROLLBARS, m_view.m_bShowScroll);

		// register object for message filtering and idle updates
		CMessageLoop* pLoop = _Module.GetMessageLoop();
		ATLASSERT(pLoop != NULL);
		pLoop->AddMessageFilter(this);
		pLoop->AddIdleHandler(this);

		// Image initialization
		LPCTSTR pCmdLine = (LPCTSTR)((LPCREATESTRUCT)lParam)->lpCreateParams;
		if ( *pCmdLine )// open the command line file
			SetImageFile( pCmdLine);
		else			// restore previous image if existing
		{
			CPoint ptScroll( 0, 0);
			double fzoom = 1.;
			info.Restore( m_sFile, L"Image");
			if ( !m_sFile.IsEmpty())
			{
				info.Restore( ptScroll, L"Scroll");
				info.Restore( fzoom, L"Zoom");
			}
			SetImageFile( m_sFile, fzoom, ptScroll);
		}
		return 0;
	}

	LRESULT OnExit(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		PostMessage(WM_CLOSE);
		return 0;
	}

// File and image transfers
	LRESULT OnFileOpen(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		LPCTSTR sFiles = 
			L"Image Files\0*.bmp;*.jpg;*.gif;*.png\0"
			L"Bitmap Files (*.bmp)\0*.bmp\0"
			L"JPEG Files (*.jpg)\0*.jpg\0"
			L"PNG Files (*.png)\0*.png\0"
			L"GIF Files (*.gif)\0*.gif\0"
			L"All Files (*.*)\0*.*\0\0";

		CFileDialog dlg( TRUE, NULL, NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, sFiles);

		if( FSDoModal( dlg) == IDOK)
			SetImageFile( dlg.m_szFileName);

		return 0;
	}

	LRESULT OnFileClose(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		SetImageFile( NULL);
		return NULL;
	}

	LRESULT OnPaste(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		if ( CBitmapHandle hbm = AtlGetClipboardDib( m_hWnd))
		{
			m_sFile.Empty();
			m_view.SetImage( hbm, L"pasted");
			UIEnable(ID_ZOOM, true);
			UIEnable(ID_FILE_CLOSE, true);
			UIEnable(ID_EDIT_COPY, true);
			UIEnable(ID_VIEW_PROPERTIES, true);
		}
		else
			AtlMessageBox( m_hWnd, L"Could not paste image from clipboard", IDR_MAINFRAME, MB_OK | MB_ICONERROR);
		return 0;
	}

// Dialogs and property sheet activation
	LRESULT OnAppAbout(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
#ifdef _WTL_CE_DRA
		CStdSimpleOrientedDialog<IDD_ABOUTBOX, IDD_ABOUTBOX_L, SHIDIF_DONEBUTTON | SHIDIF_FULLSCREENNOMENUBAR> dlg;
#else
		CAboutDlg dlg;
#endif
		return FSDoModal( dlg);
	}

	LRESULT OnRegister(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		CRegisterDlg dlg;
		return FSDoModal( dlg);
	}

	LRESULT OnProperties(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		CImageProperties prop( m_sFile, m_view);
		return FSDoModal( prop);
	}

// User interface settings
	LRESULT OnTitle(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		BOOL bView = !m_view.m_TitleBar.IsWindowVisible();
		m_view.m_TitleBar.ShowWindow( bView ? SW_SHOWNOACTIVATE : SW_HIDE);
		UpdateLayout();
		UISetCheck( ID_VIEW_STATUS_BAR, bView);
		return TRUE;
	}

	LRESULT OnFullScreen(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		SetFullScreen( !m_bFullScreen );
		UISetCheck( ID_VIEW_TOOLBAR, m_bFullScreen);
		return TRUE;
	}

	LRESULT OnScrollBars(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& bHandled)
	{
		UISetCheck( ID_VIEW_SCROLLBARS, !m_view.m_bShowScroll);
		return bHandled = FALSE; // real processing is in m_view
	}

	LRESULT OnContextMenu(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
	{
		PNMRGINFO pnmrgi = (PNMRGINFO)pnmh ;
		CMenuHandle menu;
		menu.LoadMenu( IDR_POPUP);
		menu = menu.GetSubMenu( 0);
		return menu.TrackPopupMenu( TPM_CENTERALIGN | TPM_VERTICAL, 
			pnmrgi->ptAction.x, pnmrgi->ptAction.y, m_hWnd);
	}
	
};

/////////////////////////////////////////////////////////////////////////////

#endif // !defined(_IMAGEVIEW_MAINFRM_H_)
