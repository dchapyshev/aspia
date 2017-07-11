// [!output WTL_FRAME_FILE].cpp : implmentation of the [!output WTL_FRAME_CLASS] class
//
/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
[!if WTL_USE_RIBBON]
#include "Ribbon.h"
[!endif]
#include "resource.h"

#include "aboutdlg.h"
[!if WTL_USE_VIEW]
#include "[!output WTL_VIEW_FILE].h"
[!endif]
[!if WTL_APPTYPE_MDI]
#include "[!output WTL_CHILD_FRAME_FILE].h"
[!endif]
#include "[!output WTL_FRAME_FILE].h"

BOOL [!output WTL_FRAME_CLASS]::PreTranslateMessage(MSG* pMsg)
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

BOOL [!output WTL_FRAME_CLASS]::OnIdle()
{
[!if WTL_USE_TOOLBAR]
	UIUpdateToolBar();
[!endif]
	return FALSE;
}

LRESULT [!output WTL_FRAME_CLASS]::OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
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

[!if WTL_COM_SERVER]
LRESULT [!output WTL_FRAME_CLASS]::OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
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

[!else]
LRESULT [!output WTL_FRAME_CLASS]::OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
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
LRESULT [!output WTL_FRAME_CLASS]::OnFileExit(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	PostMessage(WM_CLOSE);
	return 0;
}

LRESULT [!output WTL_FRAME_CLASS]::OnFileNew(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
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
[!if WTL_RIBBON_DUAL_UI]
LRESULT [!output WTL_FRAME_CLASS]::OnViewRibbon(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
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
LRESULT [!output WTL_FRAME_CLASS]::OnAppAbout(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	CAboutDlg dlg;
	dlg.DoModal();
	return 0;
}
[!if WTL_APPTYPE_MDI]

LRESULT [!output WTL_FRAME_CLASS]::OnWindowCascade(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	MDICascade();
	return 0;
}

LRESULT [!output WTL_FRAME_CLASS]::OnWindowTile(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	MDITile();
	return 0;
}

LRESULT [!output WTL_FRAME_CLASS]::OnWindowArrangeIcons(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	MDIIconArrange();
	return 0;
}
[!endif]
[!if WTL_APPTYPE_TABVIEW]

LRESULT [!output WTL_FRAME_CLASS]::OnWindowClose(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int nActivePage = m_view.GetActivePage();
	if(nActivePage != -1)
		m_view.RemovePage(nActivePage);
	else
		::MessageBeep((UINT)-1);

	return 0;
}

LRESULT [!output WTL_FRAME_CLASS]::OnWindowCloseAll(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	m_view.RemoveAllPages();

	return 0;
}

LRESULT [!output WTL_FRAME_CLASS]::OnWindowActivate(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int nPage = wID - ID_WINDOW_TABFIRST;
	m_view.SetActivePage(nPage);

	return 0;
}
[!endif]
[!if WTL_APPTYPE_EXPLORER]

LRESULT [!output WTL_FRAME_CLASS]::OnViewTreePane(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	bool bShow = (m_splitter.GetSinglePaneMode() != SPLIT_PANE_NONE);
	m_splitter.SetSinglePaneMode(bShow ? SPLIT_PANE_NONE : SPLIT_PANE_RIGHT);
	UISetCheck(ID_VIEW_TREEPANE, bShow);

	return 0;
}

LRESULT [!output WTL_FRAME_CLASS]::OnTreePaneClose(WORD /*wNotifyCode*/, WORD /*wID*/, HWND hWndCtl, BOOL& /*bHandled*/)
{
	if(hWndCtl == m_pane.m_hWnd)
	{
		m_splitter.SetSinglePaneMode(SPLIT_PANE_RIGHT);
		UISetCheck(ID_VIEW_TREEPANE, 0);
	}

	return 0;
}
[!endif]
