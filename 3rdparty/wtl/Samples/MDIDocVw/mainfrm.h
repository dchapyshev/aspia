#include <atlctrls.h>
#include <atlctrlw.h>

class CHelloView : public CWindowImpl<CHelloView>
{
public:
	COLORREF m_clrText;

	//state of color buttons
	BOOL m_bBlack;
	BOOL m_bRed;
	BOOL m_bBlue;
	BOOL m_bGreen;
	BOOL m_bWhite;
	BOOL m_bCustom;

	CUpdateUIBase* m_pUpdateUI;

	CHelloView(CUpdateUIBase* pUpdateUI) : m_clrText(RGB(0, 0, 0)),
		m_bBlack(TRUE),
		m_bRed(FALSE),
		m_bBlue(FALSE),
		m_bGreen(FALSE),
		m_bWhite(FALSE),
		m_bCustom(FALSE),
		m_pUpdateUI(pUpdateUI)
	{ }

	DECLARE_WND_CLASS(NULL)

	void ClearAllColors()
	{
		m_bBlack = m_bBlue = m_bRed = FALSE;
		m_bWhite = m_bGreen = FALSE;
	}

	void MixColors() 
	{
		COLORREF tmpClr;
		int r, g, b;
		BOOL bSetColor;
		
		// Determine which colors are currently chosen.
		bSetColor = m_bRed || m_bGreen || m_bBlue || m_bWhite || m_bBlack;

   		// If the current color is custom, ignore mix request.
		if(!bSetColor && m_bCustom)
			return;

   		// Set color value to black and then add the necessary colors.
		r = g = b = 0;

		if(m_bRed)
			r = 255;
		if(m_bGreen)
			g = 255;
		if(m_bBlue)
			b = 255;

		tmpClr = RGB(r, g, b);

	// NOTE: Because a simple algorithm is used to mix colors
	// if the current selection contains black or white, the 
	// result will be black or white; respectively. This is due
	// to the additive method for mixing the colors.

		if(m_bBlack)
			tmpClr = RGB(0, 0, 0);

		if(m_bWhite)
			tmpClr = RGB(255, 255, 255);

   		// Once the color has been determined, update document 
		// data, and force repaint of all views.

		if(!bSetColor)
			m_bBlack = TRUE;
		m_clrText = tmpClr;
		m_bCustom = FALSE;

		UpdateButtons();
		Invalidate();
	}

	void UpdateButtons()
	{
		m_pUpdateUI->UISetCheck(ID_BLACK, m_bBlack);
		m_pUpdateUI->UISetCheck(ID_RED, m_bRed);
		m_pUpdateUI->UISetCheck(ID_GREEN, m_bGreen);
		m_pUpdateUI->UISetCheck(ID_BLUE, m_bBlue);
		m_pUpdateUI->UISetCheck(ID_WHITE, m_bWhite);
		m_pUpdateUI->UISetCheck(ID_CUSTOM, m_bCustom);
	}

	virtual void OnFinalMessage(HWND /*hWnd*/)
	{
		delete this;
	}

	BEGIN_MSG_MAP(CHelloView)
		MESSAGE_HANDLER(WM_PAINT, OnPaint)
		MESSAGE_HANDLER(WM_SETFOCUS, OnSetFocus)
		COMMAND_RANGE_HANDLER(ID_BLACK, ID_WHITE, OnColor)
		COMMAND_ID_HANDLER(ID_CUSTOM, OnCustomColor)
	END_MSG_MAP()

	LRESULT OnPaint(UINT, WPARAM, LPARAM, BOOL&)
	{
		CPaintDC dc(m_hWnd);

		RECT rect;
		GetClientRect(&rect);
		dc.SetTextColor(m_clrText);
		dc.SetBkColor(::GetSysColor(COLOR_WINDOW));
		dc.DrawText(_T("Hello, ATL"), -1, &rect, DT_SINGLELINE | DT_CENTER | DT_VCENTER);

		return 0;
	}

	LRESULT OnSetFocus(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
	{
		m_pUpdateUI->UIEnable(ID_BLACK, TRUE);
		m_pUpdateUI->UIEnable(ID_RED, TRUE);
		m_pUpdateUI->UIEnable(ID_GREEN, TRUE);
		m_pUpdateUI->UIEnable(ID_BLUE, TRUE);
		m_pUpdateUI->UIEnable(ID_WHITE, TRUE);
		m_pUpdateUI->UIEnable(ID_CUSTOM, TRUE);

		m_pUpdateUI->UISetCheck(ID_SPEED_SLOW, 0);
		m_pUpdateUI->UISetCheck(ID_SPEED_FAST, 0);
		m_pUpdateUI->UIEnable(ID_SPEED_SLOW, FALSE);
		m_pUpdateUI->UIEnable(ID_SPEED_FAST, FALSE);

		UpdateButtons();

		bHandled = FALSE;
		return 1;
	}

	LRESULT OnColor(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
	{
		switch(wID)
		{
		 case ID_BLACK:
			ClearAllColors();
			m_bBlack = !(m_bBlack);
			break;
		 case ID_WHITE:
			ClearAllColors();
			m_bWhite = !(m_bWhite);
			break;
		 case ID_RED:
			m_bRed = !(m_bRed);
			m_bBlack = FALSE;
			m_bWhite = FALSE;
			break;
		 case ID_GREEN:
			m_bGreen = !(m_bGreen);
			m_bBlack = FALSE;
			m_bWhite = FALSE;
			break;
		 case ID_BLUE:
			m_bBlue = !(m_bBlue);
			m_bBlack = FALSE;
			m_bWhite = FALSE;
			break;
		 default:
			TCHAR buff[256];
			::LoadString(_Module.GetResourceInstance(), IDS_UNKCOLOR, buff, 255);
			MessageBox(buff);
			return 1;
		}
		MixColors();
		return 0;
	}

	LRESULT OnCustomColor(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
	{
		CColorDialog dlgColor(m_clrText);
		if(dlgColor.DoModal() == IDOK)
		{
			m_clrText = dlgColor.GetColor();
			ClearAllColors();
			m_bCustom = TRUE;
			UpdateButtons();
			Invalidate();
		}
		return 0;
	}
};

class CHelloWnd : public CMDIChildWindowImpl<CHelloWnd>
{
public:
	CHelloView* m_pView;
	CUpdateUIBase* m_pUpdateUI;

	CHelloWnd(CUpdateUIBase* pUpdateUI) : m_pView(NULL), m_pUpdateUI(pUpdateUI)
	{ }

	DECLARE_FRAME_WND_CLASS(NULL, IDR_HELLOTYPE)

	virtual void OnFinalMessage(HWND /*hWnd*/)
	{
		delete this;
	}

	BEGIN_MSG_MAP(CHelloWnd)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		CHAIN_CLIENT_COMMANDS()
		CHAIN_MSG_MAP(CMDIChildWindowImpl<CHelloWnd>)
	END_MSG_MAP()

	LRESULT OnCreate(UINT, WPARAM, LPARAM lParam, BOOL& bHandled)
	{
		m_pView = new CHelloView(m_pUpdateUI);
		RECT rect = { 0, 0, 1, 1 };
		m_hWndClient = m_pView->Create(m_hWnd, rect, NULL, WS_CHILD | WS_VISIBLE, WS_EX_CLIENTEDGE);

		bHandled = FALSE;
		return 1;
	}
};


#define ABS(x) ((x) < 0? -(x) : (x) > 0? (x) : 0)

class CBounceView : public CWindowImpl<CBounceView>
{
public:
	COLORREF m_clrBall;

	//state of color buttons
	BOOL m_bBlack;
	BOOL m_bRed;
	BOOL m_bBlue;
	BOOL m_bGreen;
	BOOL m_bWhite;
	BOOL m_bCustom;

	BOOL m_bFastSpeed;         // current speed

	POINT m_ptPixel;           // pixel size
	SIZE m_sizeRadius;         // radius of ball
	SIZE m_sizeMove;           // move speed
	SIZE m_sizeTotal;          // total size for ball bitmap
	POINT m_ptCenter;          // current center for the ball

	CBitmap m_bmBall;          // for replicating bouncing ball

	CUpdateUIBase* m_pUpdateUI;

	CBounceView(CUpdateUIBase* pUpdateUI) : m_clrBall(RGB(0, 0, 0)),
		m_bBlack(TRUE),
		m_bRed(FALSE),
		m_bBlue(FALSE),
		m_bGreen(FALSE),
		m_bWhite(FALSE),
		m_bCustom(FALSE),
		m_bFastSpeed(FALSE), m_pUpdateUI(pUpdateUI)
	{ }

	DECLARE_WND_CLASS(NULL)

	HWND Create(HWND hWndParent, RECT& rcPos, LPCTSTR szWindowName = NULL,
			DWORD dwStyle = 0, DWORD dwExStyle = 0,
			UINT nID = 0, LPVOID lpCreateParam = NULL)
	{
		CWndClassInfo& wci = GetWndClassInfo();
		wci.m_lpszCursorID = IDC_SIZENS;
		ATOM atom = wci.Register(&m_pfnSuperWindowProc);
		return CWindowImplBase::Create(hWndParent, rcPos, szWindowName, dwStyle, dwExStyle,
			nID, atom);
	}

	void ClearAllColors()
	{
		m_bBlack = m_bBlue = m_bRed = FALSE;
		m_bWhite = m_bGreen = FALSE;
	}

	void MixColors() 
	{
		COLORREF tmpClr;
		int r, g, b;
		BOOL bSetColor;
		
		// Determine which colors are currently chosen.
		bSetColor = m_bRed || m_bGreen || m_bBlue || m_bWhite || m_bBlack;

   		// If the current color is custom, ignore mix request.
		if(!bSetColor && m_bCustom)
			return;

   		// Set color value to black and then add the necessary colors.
		r = g = b = 0;

		if(m_bRed)
			r = 255;
		if(m_bGreen)
			g = 255;
		if(m_bBlue)
			b = 255;

		tmpClr = RGB(r, g, b);

	// NOTE: Because a simple algorithm is used to mix colors
	// if the current selection contains black or white, the 
	// result will be black or white; respectively. This is due
	// to the additive method for mixing the colors.

		if(m_bBlack)
			tmpClr = RGB(0, 0, 0);

		if(m_bWhite)
			tmpClr = RGB(255, 255, 255);

   		// Once the color has been determined, update document 
		// data, and force repaint of all views.

		if(!bSetColor)
			m_bBlack = TRUE;
		m_clrBall = tmpClr;
		m_bCustom = FALSE;

		UpdateColorButtons();
		MakeNewBall();
		Invalidate();
	}

	void UpdateColorButtons()
	{
		m_pUpdateUI->UISetCheck(ID_BLACK, m_bBlack);
		m_pUpdateUI->UISetCheck(ID_RED, m_bRed);
		m_pUpdateUI->UISetCheck(ID_GREEN, m_bGreen);
		m_pUpdateUI->UISetCheck(ID_BLUE, m_bBlue);
		m_pUpdateUI->UISetCheck(ID_WHITE, m_bWhite);
		m_pUpdateUI->UISetCheck(ID_CUSTOM, m_bCustom);
	}

	void UpdateSpeedButtons()
	{
		m_pUpdateUI->UISetCheck(ID_SPEED_SLOW, !m_bFastSpeed);
		m_pUpdateUI->UISetCheck(ID_SPEED_FAST, m_bFastSpeed);
	}

	void MakeNewBall()
	{
		m_sizeTotal.cx = (m_sizeRadius.cx + ABS(m_sizeMove.cx)) << 1;
		m_sizeTotal.cy = (m_sizeRadius.cy + ABS(m_sizeMove.cy)) << 1;

		if(m_bmBall.m_hBitmap != NULL)
			m_bmBall.DeleteObject();        // get rid of old bitmap

		CClientDC dc(m_hWnd);
		CDC dcMem;
		dcMem.CreateCompatibleDC(dc);

		m_bmBall.CreateCompatibleBitmap(dc, m_sizeTotal.cx, m_sizeTotal.cy);
		_ASSERTE(m_bmBall.m_hBitmap != NULL);

		dcMem.SelectBitmap(m_bmBall);

		// draw a rectangle in the background (window) color
		RECT rect = { 0, 0, m_sizeTotal.cx, m_sizeTotal.cy };
		CBrush brBackground;
		brBackground.CreateSolidBrush(::GetSysColor(COLOR_WINDOW));
		dcMem.FillRect(&rect, brBackground);

		CBrush brCross;
		brCross.CreateHatchBrush(HS_DIAGCROSS, 0L);
		dcMem.SelectBrush(brCross);

		dcMem.SetBkColor(m_clrBall);
		dcMem.Ellipse(ABS(m_sizeMove.cx), ABS(m_sizeMove.cy),
			m_sizeTotal.cx - ABS(m_sizeMove.cx),
			m_sizeTotal.cy - ABS(m_sizeMove.cy));
	}

	virtual void OnFinalMessage(HWND /*hWnd*/)
	{
		delete this;
	}

	BEGIN_MSG_MAP(CBounceView)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		MESSAGE_HANDLER(WM_SETFOCUS, OnSetFocus)
		MESSAGE_HANDLER(WM_SIZE, OnSize)
		MESSAGE_HANDLER(WM_TIMER, OnTimer)
		COMMAND_RANGE_HANDLER(ID_BLACK, ID_WHITE, OnColor)
		COMMAND_ID_HANDLER(ID_CUSTOM, OnCustomColor)
		COMMAND_ID_HANDLER(ID_SPEED_SLOW, OnChangeSpeed)
		COMMAND_ID_HANDLER(ID_SPEED_FAST, OnChangeSpeed)
	END_MSG_MAP()

	LRESULT OnCreate(UINT, WPARAM, LPARAM, BOOL& bHandled)
	{
#if (_ATL_VER >= 0x0300)
		if(!SetTimer(1, 100))	// start slow
#else
		if(!SetTimer(1, 100, NULL))	// start slow
#endif //(_ATL_VER == 0x0300)
		{
			MessageBox(_T("Not enough timers available for this window."),
					_T("MDI:Bounce"), MB_ICONEXCLAMATION | MB_OK);

			// signal creation failure...
			return -1;
		}

		CClientDC dc(m_hWnd);
		m_ptPixel.x = dc.GetDeviceCaps(ASPECTX);
		m_ptPixel.y = dc.GetDeviceCaps(ASPECTY);

		bHandled = FALSE;
		return 1;
	}

	LRESULT OnSetFocus(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
	{
		m_pUpdateUI->UIEnable(ID_BLACK, TRUE);
		m_pUpdateUI->UIEnable(ID_RED, TRUE);
		m_pUpdateUI->UIEnable(ID_GREEN, TRUE);
		m_pUpdateUI->UIEnable(ID_BLUE, TRUE);
		m_pUpdateUI->UIEnable(ID_WHITE, TRUE);
		m_pUpdateUI->UIEnable(ID_CUSTOM, TRUE);
		m_pUpdateUI->UIEnable(ID_SPEED_SLOW, TRUE);
		m_pUpdateUI->UIEnable(ID_SPEED_FAST, TRUE);

		UpdateColorButtons();
		UpdateSpeedButtons();

		bHandled = FALSE;
		return 1;
	}

	LRESULT OnSize(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
	{
		int cx = LOWORD(lParam);
		int cy = HIWORD(lParam);
		LONG lScale;

		m_ptCenter.x = cx >> 1;
		m_ptCenter.y = cy >> 1;
		m_ptCenter.x += cx >> 3; // make the ball a little off-center

		lScale = min((LONG)cx * m_ptPixel.x,
			(LONG)cy * m_ptPixel.y) >> 4;
		m_sizeRadius.cx = (int)(lScale / m_ptPixel.x);
		m_sizeRadius.cy = (int)(lScale / m_ptPixel.y);
		m_sizeMove.cx = max(1, m_sizeRadius.cy >> 2);
		m_sizeMove.cy = max(1, m_sizeRadius.cy >> 2);

		MakeNewBall();

		bHandled = FALSE;
		return 1;
	}

	LRESULT OnTimer(UINT, WPARAM, LPARAM, BOOL& bHandled)
	{
		if (m_bmBall.m_hBitmap == NULL)
			return 1;     // no bitmap for the ball

		RECT rcClient;
		GetClientRect(&rcClient);

		CClientDC dc(m_hWnd);

		CDC dcMem;
		dcMem.CreateCompatibleDC(dc);
		dcMem.SelectBitmap(m_bmBall);

		dc.BitBlt(m_ptCenter.x - m_sizeTotal.cx / 2,
				m_ptCenter.y - m_sizeTotal.cy / 2,
				m_sizeTotal.cx, m_sizeTotal.cy,
				dcMem, 0, 0, SRCCOPY);

		m_ptCenter.x += m_sizeMove.cx;
		m_ptCenter.y += m_sizeMove.cy;

		if((m_ptCenter.x + m_sizeRadius.cx >= rcClient.right) ||
			(m_ptCenter.x - m_sizeRadius.cx <= 0))
		{
			m_sizeMove.cx = -m_sizeMove.cx;
		}

		if((m_ptCenter.y + m_sizeRadius.cy >= rcClient.bottom) ||
			(m_ptCenter.y - m_sizeRadius.cy <= 0))
		{
			m_sizeMove.cy = -m_sizeMove.cy;
		}

		return 0;
	}

	LRESULT OnColor(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
	{
		switch(wID)
		{
		 case ID_BLACK:
			ClearAllColors();
			m_bBlack = !(m_bBlack);
			break;
		 case ID_WHITE:
			ClearAllColors();
			m_bWhite = !(m_bWhite);
			break;
		 case ID_RED:
			m_bRed = !(m_bRed);
			m_bBlack = FALSE;
			m_bWhite = FALSE;
			break;
		 case ID_GREEN:
			m_bGreen = !(m_bGreen);
			m_bBlack = FALSE;
			m_bWhite = FALSE;
			break;
		 case ID_BLUE:
			m_bBlue = !(m_bBlue);
			m_bBlack = FALSE;
			m_bWhite = FALSE;
			break;
		 default:
			TCHAR buff[256];
			::LoadString(_Module.GetResourceInstance(), IDS_UNKCOLOR, buff, 255);
			MessageBox(buff);
			return 1;
		}
		MixColors();
		return 0;
	}

	LRESULT OnCustomColor(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
	{
		CColorDialog dlgColor(m_clrBall);
		if(dlgColor.DoModal() == IDOK)
		{
			m_clrBall = dlgColor.GetColor();
			ClearAllColors();
			m_bCustom = TRUE;
			UpdateColorButtons();
			MakeNewBall();
			Invalidate();
		}
		return 0;
	}

	LRESULT OnChangeSpeed(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
	{
		m_bFastSpeed = (wID == ID_SPEED_FAST) ? TRUE : FALSE;
		UpdateSpeedButtons();
		KillTimer(1);
#if (_ATL_VER >= 0x0300)
		if(!SetTimer(1, m_bFastSpeed ? 0 : 100))
#else
		if(!SetTimer(1, m_bFastSpeed ? 0 : 100, NULL))
#endif //(_ATL_VER == 0x0300)
		{
			MessageBox(_T("Not enough timers available for this window."),
					_T("MDI:Bounce"), MB_ICONEXCLAMATION | MB_OK);
			DestroyWindow();
		}
		return 0;
	}
};

class CBounceWnd : public CMDIChildWindowImpl<CBounceWnd>
{
public:
	CBounceView* m_pView;
	CUpdateUIBase* m_pUpdateUI;

	CBounceWnd(CUpdateUIBase* pUpdateUI) : m_pView(NULL), m_pUpdateUI(pUpdateUI)
	{ }

	DECLARE_FRAME_WND_CLASS(NULL, IDR_BOUNCETYPE)

	HWND Create(HWND hWndParent, _U_RECT rect = NULL, LPCTSTR szWindowName = NULL,
			DWORD dwStyle = 0, DWORD dwExStyle = 0,
			UINT nMenuID = 0, LPVOID lpCreateParam = NULL)
	{
		CFrameWndClassInfo& wci = GetWndClassInfo();
		wci.m_lpszCursorID = IDC_UPARROW;
		return CMDIChildWindowImpl<CBounceWnd>::Create(hWndParent, rect, szWindowName, dwStyle, dwExStyle, nMenuID, lpCreateParam);
	}

	virtual void OnFinalMessage(HWND /*hWnd*/)
	{
		delete this;
	}

	BEGIN_MSG_MAP(CBounceWnd)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		CHAIN_CLIENT_COMMANDS()
		CHAIN_MSG_MAP(CMDIChildWindowImpl<CBounceWnd>)
	END_MSG_MAP()

	LRESULT OnCreate(UINT, WPARAM, LPARAM, BOOL& bHandled)
	{
		m_pView = new CBounceView(m_pUpdateUI);
		RECT rect = { 0, 0, 1, 1 };
		m_hWndClient = m_pView->Create(m_hWnd, rect, NULL, WS_CHILD | WS_VISIBLE, WS_EX_CLIENTEDGE);

		bHandled = FALSE;
		return 1;
	}
};


class CAboutDlg : public CDialogImpl<CAboutDlg>
{
public:
	enum { IDD = IDD_ABOUTDLG };

	BEGIN_MSG_MAP(CAboutDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_ID_HANDLER(IDOK, OnCloseCmd)
		COMMAND_ID_HANDLER(IDCANCEL, OnCloseCmd)
	END_MSG_MAP()

	LRESULT OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
	{
		CenterWindow(GetParent());
		return (LRESULT)TRUE;
	}

	LRESULT OnCloseCmd(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
	{
		EndDialog(wID);
		return 0;
	}
};


class CMDIFrame : public CMDIFrameWindowImpl<CMDIFrame>, CUpdateUI<CMDIFrame>,
		public CMessageFilter, public CUpdateUIObject
{
public:
	DECLARE_FRAME_WND_CLASS(NULL, IDR_MAINFRAME)

	CMDICommandBarCtrl m_CmdBar;

	virtual BOOL PreTranslateMessage(MSG* pMsg)
	{
		return CMDIFrameWindowImpl<CMDIFrame>::PreTranslateMessage(pMsg);
	}

	virtual BOOL DoUpdate()
	{
		UIUpdateToolBar();
		return FALSE;
	}

	BEGIN_MSG_MAP(CMDIFrame)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		COMMAND_ID_HANDLER(ID_APP_EXIT, OnFileExit)
		COMMAND_ID_HANDLER(ID_FILE_NEWBOUNCE, OnBounce)
		COMMAND_ID_HANDLER(ID_FILE_NEWHELLO, OnHello)
		COMMAND_ID_HANDLER(ID_WINDOW_CASCADE, OnWindowCascade)
		COMMAND_ID_HANDLER(ID_WINDOW_TILE_HORZ, OnWindowTile)
		COMMAND_ID_HANDLER(ID_WINDOW_ARRANGE, OnWindowArrangeIcons)
		COMMAND_ID_HANDLER(ID_VIEW_TOOLBAR, OnViewToolBar)
		COMMAND_ID_HANDLER(ID_VIEW_STATUS_BAR, OnViewStatusBar)
		COMMAND_ID_HANDLER(ID_APP_ABOUT, OnAppAbout)
		MESSAGE_HANDLER(WM_MDISETMENU, OnMDISetMenu)
		CHAIN_MDI_CHILD_COMMANDS()
		CHAIN_MSG_MAP(CUpdateUI<CMDIFrame>)
		CHAIN_MSG_MAP(CMDIFrameWindowImpl<CMDIFrame>)
	END_MSG_MAP()

	BEGIN_UPDATE_UI_MAP(CMDIFrame)
		UPDATE_ELEMENT(ID_VIEW_TOOLBAR, UPDUI_MENUPOPUP)
		UPDATE_ELEMENT(ID_VIEW_STATUS_BAR, UPDUI_MENUPOPUP)
		UPDATE_ELEMENT(ID_BLACK,  UPDUI_MENUPOPUP | UPDUI_TOOLBAR)
		UPDATE_ELEMENT(ID_RED,    UPDUI_MENUPOPUP | UPDUI_TOOLBAR)
		UPDATE_ELEMENT(ID_GREEN,  UPDUI_MENUPOPUP | UPDUI_TOOLBAR)
		UPDATE_ELEMENT(ID_BLUE,   UPDUI_MENUPOPUP | UPDUI_TOOLBAR)
		UPDATE_ELEMENT(ID_WHITE,  UPDUI_MENUPOPUP | UPDUI_TOOLBAR)
		UPDATE_ELEMENT(ID_CUSTOM, UPDUI_MENUPOPUP | UPDUI_TOOLBAR)
		UPDATE_ELEMENT(ID_SPEED_SLOW, UPDUI_MENUPOPUP | UPDUI_TOOLBAR)
		UPDATE_ELEMENT(ID_SPEED_FAST, UPDUI_MENUPOPUP | UPDUI_TOOLBAR)
	END_UPDATE_UI_MAP()

	LRESULT OnCreate(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
	{
		// Create MDIClient
		CreateMDIClient();

		// Create MDI CommandBar
		m_CmdBar.Create(m_hWnd, rcDefault, NULL, ATL_SIMPLE_CMDBAR_PANE_STYLE);
		m_CmdBar.SetMDIClient(m_hWndClient);
		m_CmdBar.AttachMenu(GetMenu());
		SetMenu(NULL);

		// Create Toolbar
		HWND hWndToolBar = CreateSimpleToolBarCtrl(m_hWnd, IDR_MAINFRAME, FALSE, ATL_SIMPLE_TOOLBAR_PANE_STYLE);
		UIAddToolBar(hWndToolBar);

		// Create Rebar
		CreateSimpleReBar(ATL_SIMPLE_REBAR_NOBORDER_STYLE);
		AddSimpleReBarBand(m_CmdBar);
		AddSimpleReBarBand(hWndToolBar, NULL, TRUE);

		CreateSimpleStatusBar(_T("Ready"));

		UISetCheck(ID_VIEW_TOOLBAR, 1);
		UISetCheck(ID_VIEW_STATUS_BAR, 1);

		UIEnable(ID_BLACK, FALSE);
		UIEnable(ID_RED, FALSE);
		UIEnable(ID_GREEN, FALSE);
		UIEnable(ID_BLUE, FALSE);
		UIEnable(ID_WHITE, FALSE);
		UIEnable(ID_CUSTOM, FALSE);
		UIEnable(ID_SPEED_SLOW, FALSE);
		UIEnable(ID_SPEED_FAST, FALSE);

		CMessageLoop* pLoop = _Module.GetMessageLoop();
		pLoop->AddMessageFilter(this);
		pLoop->AddUpdateUI(this);

		return 0;
	}


	LRESULT OnMDISetMenu(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
	{
		UISetCheck(ID_BLACK, 0);
		UISetCheck(ID_RED, 0);
		UISetCheck(ID_GREEN, 0);
		UISetCheck(ID_BLUE, 0);
		UISetCheck(ID_WHITE, 0);
		UISetCheck(ID_CUSTOM, 0);
		UISetCheck(ID_SPEED_SLOW, 0);
		UISetCheck(ID_SPEED_FAST, 0);

		UIEnable(ID_BLACK, FALSE);
		UIEnable(ID_RED, FALSE);
		UIEnable(ID_GREEN, FALSE);
		UIEnable(ID_BLUE, FALSE);
		UIEnable(ID_WHITE, FALSE);
		UIEnable(ID_CUSTOM, FALSE);
		UIEnable(ID_SPEED_SLOW, FALSE);
		UIEnable(ID_SPEED_FAST, FALSE);

		bHandled = FALSE;
		return 1;
	}

	LRESULT OnFileExit(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
	{
		PostMessage(WM_CLOSE);
		return 0;
	}

	LRESULT OnBounce(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
	{
		CBounceWnd* pChild = new CBounceWnd((CUpdateUIBase*)this);
		pChild->CreateEx(m_hWndClient, NULL, _T("Bounce"));

		return 0;
	}

	LRESULT OnHello(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
	{
		CHelloWnd* pChild = new CHelloWnd((CUpdateUIBase*)this);
		pChild->CreateEx(m_hWndClient, NULL, _T("Hello"));

		return 0;
	}

	LRESULT OnWindowCascade(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
	{
		MDICascade(0);
		return 0;
	}

	LRESULT OnWindowTile(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
	{
		MDITile(MDITILE_HORIZONTAL);
		return 0;
	}

	LRESULT OnWindowArrangeIcons(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
	{
		MDIIconArrange();
		return 0;
	}

	LRESULT OnViewToolBar(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
	{
		BOOL bNew = !::IsWindowVisible(m_hWndToolBar);
		::ShowWindow(m_hWndToolBar, bNew ? SW_SHOWNOACTIVATE : SW_HIDE);
		UISetCheck(ID_VIEW_TOOLBAR, bNew);
		UpdateLayout();
		return 0;
	}

	LRESULT OnViewStatusBar(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
	{
		BOOL bNew = !::IsWindowVisible(m_hWndStatusBar);
		::ShowWindow(m_hWndStatusBar, bNew ? SW_SHOWNOACTIVATE : SW_HIDE);
		UISetCheck(ID_VIEW_STATUS_BAR, bNew);
		UpdateLayout();
		return 0;
	}

	LRESULT OnAppAbout(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
	{
		CAboutDlg dlg;
		dlg.DoModal();
		return 0;
	}
};
