// MiniPieFrame.h : interface of the CMiniPieFrame class
//
/////////////////////////////////////////////////////////////////////////////

#pragma once

#define ID_TITLE 0		// Document title uses StatusBar pane 0  
#define ID_BROWSER 1	// DispEvent ID

class CMiniPieFrame : 
	public CFrameWindowImpl<CMiniPieFrame>, 
	public CUpdateUI<CMiniPieFrame>,
	public CAppWindow<CMiniPieFrame>,
	public CFullScreenFrame<CMiniPieFrame>,
	public CMessageFilter, public CIdleHandler,
    public IDispEventImpl<ID_BROWSER, CMiniPieFrame>
{
public:
	DECLARE_APP_FRAME_CLASS(NULL, IDR_MAINFRAME, L"Software\\WTL\\MiniPie")

// Data members
    CAxWindow m_browser;
    CComPtr<IWebBrowser2> m_spIWebBrowser2;

// Message filter
	virtual BOOL PreTranslateMessage(MSG* pMsg);

// CAppWindow operations
	bool AppNewInstance( LPCTSTR lpstrCmdLine);
	void AppSave();

// Implementation
	void UpdateLayout(BOOL bResizeBars = TRUE);
	BOOL SetCommandButton(INT iID, bool bRight = false);
	void Navigate(LPCTSTR sUrl);

// Idle handler and UI map
	virtual BOOL OnIdle();

	BEGIN_UPDATE_UI_MAP(CMiniPieFrame)
		UPDATE_ELEMENT(ID_TITLE, UPDUI_STATUSBAR)
		UPDATE_ELEMENT(ID_VIEW_FULLSCREEN, UPDUI_MENUPOPUP)
		UPDATE_ELEMENT(ID_VIEW_STATUS_BAR, UPDUI_MENUPOPUP)
		UPDATE_ELEMENT(ID_VIEW_ADDRESSBAR, UPDUI_MENUPOPUP)
		UPDATE_ELEMENT(IDM_BACK, UPDUI_MENUPOPUP)
		UPDATE_ELEMENT(IDM_FORWARD, UPDUI_MENUPOPUP)
		UPDATE_ELEMENT(IDM_STOP, UPDUI_MENUPOPUP | UPDUI_TOOLBAR)
		UPDATE_ELEMENT(IDM_REFRESH, UPDUI_MENUPOPUP | UPDUI_TOOLBAR)
	END_UPDATE_UI_MAP()

// Message map and handlers
	BEGIN_MSG_MAP(CMiniPieFrame)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		COMMAND_ID_HANDLER(ID_APP_EXIT, OnFileExit)
		COMMAND_ID_HANDLER(ID_VIEW_FULLSCREEN, OnFullScreen)
		COMMAND_ID_HANDLER(ID_VIEW_STATUS_BAR, OnViewStatusBar)
#ifdef WIN32_PLATFORM_PSPC
		COMMAND_ID_HANDLER(ID_VIEW_ADDRESSBAR, OnViewAddressBar)
#endif
		COMMAND_ID_HANDLER(ID_APP_ABOUT, OnAppAbout)
		COMMAND_RANGE_HANDLER(IDM_FORWARD, IDM_STOP, OnBrowserCmd)
		COMMAND_ID_HANDLER(IDM_OPENURL, OnOpenURL)
		CHAIN_MSG_MAP(CAppWindow<CMiniPieFrame>)
		CHAIN_MSG_MAP(CFullScreenFrame<CMiniPieFrame>)
		CHAIN_MSG_MAP(CUpdateUI<CMiniPieFrame>)
		CHAIN_MSG_MAP(CFrameWindowImpl<CMiniPieFrame>)
	END_MSG_MAP()

	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnFileExit(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnFullScreen(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnViewStatusBar(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
#ifdef WIN32_PLATFORM_PSPC
	LRESULT OnViewAddressBar(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
#endif
	LRESULT OnAppAbout(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnBrowserCmd(WORD /*wNotifyCode*/, WORD wID/**/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnOpenURL(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

// IWebBrowser2 event map and handlers
	BEGIN_SINK_MAP(CMiniPieFrame)
        SINK_ENTRY(ID_BROWSER, DISPID_BEFORENAVIGATE2, OnBeforeNavigate2)
        SINK_ENTRY(ID_BROWSER, DISPID_TITLECHANGE, OnBrowserTitleChange)
        SINK_ENTRY(ID_BROWSER, DISPID_NAVIGATECOMPLETE2, OnNavigateComplete2)
        SINK_ENTRY(ID_BROWSER, DISPID_DOCUMENTCOMPLETE, OnDocumentComplete)
        SINK_ENTRY(ID_BROWSER, DISPID_COMMANDSTATECHANGE, OnCommandStateChange)
    END_SINK_MAP()

private:
    void __stdcall OnBeforeNavigate2(IDispatch* pDisp, VARIANT * pvtURL, 
                                     VARIANT * /*pvtFlags*/, VARIANT * pvtTargetFrameName,
                                     VARIANT * /*pvtPostData*/, VARIANT * /*pvtHeaders*/, 
                                     VARIANT_BOOL * /*pvbCancel*/);
    void __stdcall OnBrowserTitleChange(BSTR bstrTitleText);
    void __stdcall OnNavigateComplete2(IDispatch* pDisp, VARIANT * pvtURL);
    void __stdcall OnDocumentComplete(IDispatch* pDisp, VARIANT * pvtURL);
    void __stdcall OnCommandStateChange(long lCommand, BOOL bEnable);
};
