// MiniPieFrame.cpp : implementation of the CMiniPieFrame class
//
/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#ifdef WIN32_PLATFORM_PSPC
#include "resourceppc.h"
#else
#include "resourcesp.h"
#endif

#include "UrlDlg.h"
#include "MiniPieFrame.h"

BOOL CMiniPieFrame::PreTranslateMessage(MSG* pMsg)
{
	if(CFrameWindowImpl<CMiniPieFrame>::PreTranslateMessage(pMsg))
		return TRUE; 

	if((pMsg->message < WM_KEYFIRST || pMsg->message > WM_KEYLAST) &&
	   (pMsg->message < WM_MOUSEFIRST || pMsg->message > WM_MOUSELAST))
		return FALSE;

	if (m_bFullScreen && pMsg->message == WM_KEYUP && 
		(pMsg->wParam == VK_F1 ||  pMsg->wParam == VK_F2))
		SetFullScreen(false);

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
}

bool CMiniPieFrame::AppNewInstance(LPCTSTR lpstrCmdLine)
{
	Navigate(lpstrCmdLine);
	return true;
}

void CMiniPieFrame::AppSave()
{
	CAppInfo info;
	info.Save( m_bFullScreen, L"Full");
	bool bStatus = (UIGetState(ID_VIEW_STATUS_BAR) & UPDUI_CHECKED) == UPDUI_CHECKED;
	info.Save(bStatus, L"Status");
#ifdef WIN32_PLATFORM_PSPC
    VARIANT_BOOL vb;
	m_spIWebBrowser2->get_AddressBar(&vb);
	info.Save(vb, L"Address");
#endif
}

void CMiniPieFrame::UpdateLayout(BOOL bResizeBars)
{
	RECT rect = { 0 };
	GetClientRect(&rect);

	// resize status bar
	if(m_hWndStatusBar != NULL && ((DWORD)::GetWindowLong(m_hWndStatusBar, GWL_STYLE) & WS_VISIBLE))
	{
		if(bResizeBars)
			::SendMessage(m_hWndStatusBar, WM_SIZE, 0, 0);
		RECT rectSB = { 0 };
		::GetWindowRect(m_hWndStatusBar, &rectSB);
		rect.top += rectSB.bottom - rectSB.top;
	}
	// resize client window
	if(m_hWndClient != NULL)
		::SetWindowPos(m_hWndClient, NULL, rect.left, rect.top,
			rect.right - rect.left, rect.bottom - rect.top,
			SWP_NOZORDER | SWP_NOACTIVATE);
}

BOOL CMiniPieFrame::OnIdle()
{
	UIUpdateToolBar();
	UIUpdateStatusBar();
	return FALSE;
}

BOOL CMiniPieFrame::SetCommandButton(INT iID, bool bRight)
{
	TBBUTTONINFO tbbi = { sizeof(TBBUTTONINFO), TBIF_TEXT | TBIF_COMMAND | TBIF_BYINDEX };
	tbbi.idCommand =  iID;
	tbbi.pszText = (LPTSTR)AtlLoadString(iID);
	return CMenuBarCtrl(m_hWndCECommandBar).SetButtonInfo(bRight, &tbbi);
}

void CMiniPieFrame::Navigate(LPCTSTR sUrl)
{
	CComBSTR bsUrl = sUrl;
    m_spIWebBrowser2->Navigate(bsUrl, NULL, NULL, NULL, NULL);
}

LRESULT CMiniPieFrame::OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	CAppInfo info;

	// Full screen mode delayed restoration 
	bool bFull = false;
	info.Restore(bFull, L"Full");
	if (bFull)
		PostMessage(WM_COMMAND, ID_VIEW_FULLSCREEN);

	CreateSimpleCEMenuBar();
#ifdef WIN32_PLATFORM_WFSP // SmartPhone
	AtlActivateBackKey(m_hWndCECommandBar);
#endif 
	UIAddToolBar(m_hWndCECommandBar);
	SetCommandButton(ID_APP_EXIT);

	// StatusBar state restoration 
	bool bVisible = true;
	info.Restore(bVisible, L"Status");
	DWORD dwStyle = WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | CCS_TOP;
	if (bVisible)
		dwStyle |= WS_VISIBLE;

	// StatusBar creation 
	CreateSimpleStatusBar(ATL_IDS_IDLEMESSAGE, dwStyle);
	UIAddStatusBar(m_hWndStatusBar);
	UISetCheck(ID_VIEW_STATUS_BAR, bVisible);

	// Browser view creation
	m_hWndClient = m_browser.Create(m_hWnd, NULL, _T("Microsoft.PIEDocView"),
                     WS_CHILD | WS_VISIBLE | WS_BORDER, 0, ID_BROWSER);

    ATLVERIFY(SUCCEEDED(m_browser.QueryControl(&m_spIWebBrowser2)));
    ATLVERIFY(SUCCEEDED(AtlAdviseSinkMap(this, true)));

	// Navigation menu initialization
	UIEnable(IDM_BACK, FALSE);
	UIEnable(IDM_FORWARD, FALSE);
	UIEnable(IDM_STOP, FALSE);
	UIEnable(IDM_REFRESH, FALSE);

#ifdef WIN32_PLATFORM_PSPC 
	// PPC Address bar state restoration
    VARIANT_BOOL vb = ATL_VARIANT_TRUE;
	info.Restore(vb, L"Address");
	m_spIWebBrowser2->put_AddressBar(vb);
	UISetCheck(ID_VIEW_ADDRESSBAR, vb == ATL_VARIANT_TRUE);
#endif 

	// register object for message filtering and idle updates
	CMessageLoop* pLoop = _Module.GetMessageLoop();
	ATLASSERT(pLoop != NULL);
	pLoop->AddMessageFilter(this);
	pLoop->AddIdleHandler(this);

	return 0;
}

LRESULT CMiniPieFrame::OnFileExit(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
    ATLVERIFY(SUCCEEDED(AtlAdviseSinkMap(this, false)));
	PostMessage(WM_CLOSE);
	return 0;
}

LRESULT CMiniPieFrame::OnFullScreen(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	SetFullScreen(!m_bFullScreen );
	return TRUE;
}

LRESULT CMiniPieFrame::OnViewStatusBar(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	BOOL bVisible = !::IsWindowVisible(m_hWndStatusBar);
	::ShowWindow(m_hWndStatusBar, bVisible ? SW_SHOWNOACTIVATE : SW_HIDE);
	UISetCheck(ID_VIEW_STATUS_BAR, bVisible);
	UpdateLayout();
	return 0;
}

#ifdef WIN32_PLATFORM_PSPC
LRESULT CMiniPieFrame::OnViewAddressBar(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
    VARIANT_BOOL vb;
	m_spIWebBrowser2->get_AddressBar(&vb);
	m_spIWebBrowser2->put_AddressBar(vb == ATL_VARIANT_TRUE ? ATL_VARIANT_FALSE : ATL_VARIANT_TRUE);
	UISetCheck(ID_VIEW_ADDRESSBAR, vb == ATL_VARIANT_FALSE);
	return 0;
}

// Specialization: create a PPC about dialog menubar 
LRESULT CStdSimpleDialog<IDD_ABOUTBOX>::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	CreateMenuBar(ATL_IDM_MENU_DONE);
	StdPlatformInit();
	StdShidInit();
	return TRUE;
}
#endif

LRESULT CMiniPieFrame::OnAppAbout(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	CStdSimpleDialog<IDD_ABOUTBOX> dlg;
	FSDoModal(dlg);
	return 0;
}

LRESULT CMiniPieFrame::OnOpenURL(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
    CUrlDlg dlg;
    if (FSDoModal(dlg) == IDOK)
		Navigate(dlg.GetUrl());
	return 0;
}

LRESULT CMiniPieFrame::OnBrowserCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	HRESULT (IWebBrowser::*cmd[])(void)  = 
	{
		&IWebBrowser::GoForward, // wID == IDM_FORWARD
		&IWebBrowser::GoBack, 
		&IWebBrowser::GoHome, 
		&IWebBrowser::Refresh, 
		&IWebBrowser::Stop
	};

	(m_spIWebBrowser2->*cmd[wID - IDM_FORWARD])();
	return 0;
}

// IWebBrowser2 event handlers

void __stdcall CMiniPieFrame::OnBeforeNavigate2(IDispatch* /*pDisp*/, VARIANT * /*pvtURL*/, 
                                              VARIANT * /*pvtFlags*/, VARIANT * /*pvtTargetFrameName*/,
                                              VARIANT * /*pvtPostData*/, VARIANT * /*pvtHeaders*/, 
                                              VARIANT_BOOL * /*pvbCancel*/)
{
    UISetText(ID_TITLE, L"Loading ...");
	SetCommandButton(IDM_STOP);
    UIEnable(IDM_STOP, TRUE);
    UIEnable(IDM_REFRESH, FALSE);
}

void __stdcall CMiniPieFrame::OnBrowserTitleChange(BSTR bstrTitleText)
{
	CString sTitle;
	sTitle.Format(L"\t%s\t", bstrTitleText);
    UISetText(ID_TITLE, sTitle); 
}

void __stdcall CMiniPieFrame::OnNavigateComplete2(IDispatch* /*pDisp*/, VARIANT * /*pvtURL*/)
{
    UIEnable(IDM_REFRESH, TRUE);
}

void __stdcall CMiniPieFrame::OnDocumentComplete(IDispatch* /*pDisp*/, VARIANT * /*pvtURL*/)
{
	SetCommandButton(IDM_REFRESH);
    UIEnable(IDM_STOP, FALSE);
}

void __stdcall CMiniPieFrame::OnCommandStateChange(long lCommand, BOOL bEnable)
{
	UIEnable(IDM_FORWARD - CSC_NAVIGATEFORWARD + lCommand, bEnable);
}
