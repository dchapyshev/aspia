// [!output WTL_APPWND_FILE].h : interface of the [!output WTL_MAINDLG_CLASS] class
//
/////////////////////////////////////////////////////////////////////////////

#pragma once

class [!output WTL_MAINDLG_CLASS] : 
	public [!output WTL_MAINDLG_BASE_CLASS]<[!output WTL_MAINDLG_CLASS]>,
	public CUpdateUI<[!output WTL_MAINDLG_CLASS]>,
	public CMessageFilter, public CIdleHandler
{
public:
	DECLARE_APP_DLG_CLASS(NULL, IDR_MAINFRAME, L"Software\\WTL\\[!output NICE_SAFE_PROJECT_NAME]")

[!if WTL_APP_DLG_ORIENT]
	enum { IDD = IDD_MAINDLG, IDD_LANDSCAPE = IDD_MAINDLG_L };
[!else]
	enum { IDD = IDD_MAINDLG };
[!endif]

	virtual BOOL PreTranslateMessage(MSG* pMsg)
	{
[!if WTL_ENABLE_AX]
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

[!endif]
		return CWindow::IsDialogMessage(pMsg);
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
		// Insert your code here or delete member if not relevant
	}

//?sp
	void AppBackKey() 
	{
		StdCloseDialog(IDCANCEL);
	}
//?end

	virtual BOOL OnIdle()
	{
		UIUpdateChildWindows();
		return FALSE;
	}

	BEGIN_UPDATE_UI_MAP([!output WTL_MAINDLG_CLASS])
	END_UPDATE_UI_MAP()

[!if WTL_APP_DLG_RESIZE]
	BEGIN_DLGRESIZE_MAP([!output WTL_MAINDLG_CLASS])
		DLGRESIZE_CONTROL(IDC_INFOSTATIC, DLSZ_SIZE_X | DLSZ_SIZE_Y)
		DLGRESIZE_CONTROL(ID_APP_ABOUT, DLSZ_MOVE_X | DLSZ_MOVE_Y)
	END_DLGRESIZE_MAP()
[!endif]

	BEGIN_MSG_MAP([!output WTL_MAINDLG_CLASS])
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
[!if WTL_APP_DLG_RESIZE]
//?ppc
		MESSAGE_HANDLER(WM_SETTINGCHANGE, OnSettingChange)
//?end
[!endif]
		COMMAND_ID_HANDLER(ID_APP_ABOUT, OnAppAbout)
		CHAIN_MSG_MAP([!output WTL_MAINDLG_BASE_CLASS]<[!output WTL_MAINDLG_CLASS]>)
	END_MSG_MAP()

// Handler prototypes (uncomment arguments if needed):
//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
	{

		HWND hMenuBar = CreateMenuBar(ATL_IDM_MENU_DONECANCEL);
		UIAddToolBar(hMenuBar);
		UIAddChildWindowContainer(m_hWnd);

		// register object for message filtering and idle updates
		CMessageLoop* pLoop = _Module.GetMessageLoop();
		ATLASSERT(pLoop != NULL);
		pLoop->AddMessageFilter(this);
		pLoop->AddIdleHandler(this);

		return bHandled = FALSE;
	}

[!if WTL_APP_DLG_RESIZE]
//?ppc
	LRESULT OnSettingChange(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	{
		// prevent resizing on SIP change
		return 0;
	}
//?end

[!endif]
	LRESULT OnAppAbout(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		CAboutDlg dlg;
		dlg.DoModal();
		return 0;
	}

};

