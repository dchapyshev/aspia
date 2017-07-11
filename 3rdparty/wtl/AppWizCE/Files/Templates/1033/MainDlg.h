// [!output WTL_MAINDLG_FILE].h : interface of the [!output WTL_MAINDLG_CLASS] class
//
/////////////////////////////////////////////////////////////////////////////

#pragma once

[!if WTL_APPTYPE_DLG && !WTL_APPTYPE_DLG_MODAL]
class [!output WTL_MAINDLG_CLASS] : public [!output WTL_MAINDLG_BASE_CLASS]<[!output WTL_MAINDLG_CLASS]>, public CUpdateUI<[!output WTL_MAINDLG_CLASS]>,
		public CMessageFilter, public CIdleHandler
{
public:
	enum { IDD = IDD_MAINDLG };

[!if WTL_USE_CPP_FILES]
	virtual BOOL PreTranslateMessage(MSG* pMsg);
[!else]
	virtual BOOL PreTranslateMessage(MSG* pMsg)
	{
[!if WTL_HOST_AX]
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

[!endif]
[!if WTL_USE_CPP_FILES]
	virtual BOOL OnIdle();
[!else]
	virtual BOOL OnIdle()
	{
		UIUpdateChildWindows();
		return FALSE;
	}
[!endif]

	BEGIN_UPDATE_UI_MAP([!output WTL_MAINDLG_CLASS])
	END_UPDATE_UI_MAP()

	BEGIN_MSG_MAP([!output WTL_MAINDLG_CLASS])
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
[!if WTL_COM_SERVER]
		MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
[!endif]
		COMMAND_ID_HANDLER(ID_APP_ABOUT, OnAppAbout)
		COMMAND_ID_HANDLER(IDOK, OnOK)
		COMMAND_ID_HANDLER(IDCANCEL, OnCancel)
	END_MSG_MAP()

// Handler prototypes (uncomment arguments if needed):
//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

[!if WTL_USE_CPP_FILES]
	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
[!else]
	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	{
		// center the dialog on the screen
		CenterWindow();

		// set icons
		HICON hIcon = AtlLoadIconImage(IDR_MAINFRAME, LR_DEFAULTCOLOR, ::GetSystemMetrics(SM_CXICON), ::GetSystemMetrics(SM_CYICON));
		SetIcon(hIcon, TRUE);
		HICON hIconSmall = AtlLoadIconImage(IDR_MAINFRAME, LR_DEFAULTCOLOR, ::GetSystemMetrics(SM_CXSMICON), ::GetSystemMetrics(SM_CYSMICON));
		SetIcon(hIconSmall, FALSE);

		// register object for message filtering and idle updates
		CMessageLoop* pLoop = _Module.GetMessageLoop();
		ATLASSERT(pLoop != NULL);
		pLoop->AddMessageFilter(this);
		pLoop->AddIdleHandler(this);

		UIAddChildWindowContainer(m_hWnd);

		return TRUE;
	}

[!endif]
[!if WTL_COM_SERVER]
[!if WTL_USE_CPP_FILES]
	LRESULT OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
[!else]
	LRESULT OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	{
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
[!if WTL_USE_CPP_FILES]
	LRESULT OnOK(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
[!else]
	LRESULT OnOK(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		// TODO: Add validation code 
		CloseDialog(wID);
		return 0;
	}

[!endif]
[!if WTL_USE_CPP_FILES]
	LRESULT OnCancel(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
[!else]
	LRESULT OnCancel(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		CloseDialog(wID);
		return 0;
	}

[!endif]
[!if WTL_USE_CPP_FILES]

	void CloseDialog(int nVal);
[!else]
	void CloseDialog(int nVal)
	{
		DestroyWindow();
		::PostQuitMessage(nVal);
	}
[!endif]
};
[!endif]
[!if WTL_APPTYPE_DLG && WTL_APPTYPE_DLG_MODAL]
class [!output WTL_MAINDLG_CLASS] : public [!output WTL_MAINDLG_BASE_CLASS]<[!output WTL_MAINDLG_CLASS]>
{
public:
	enum { IDD = IDD_MAINDLG };

	BEGIN_MSG_MAP([!output WTL_MAINDLG_CLASS])
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
[!if WTL_COM_SERVER]
		MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
[!endif]
		COMMAND_ID_HANDLER(ID_APP_ABOUT, OnAppAbout)
		COMMAND_ID_HANDLER(IDOK, OnOK)
		COMMAND_ID_HANDLER(IDCANCEL, OnCancel)
	END_MSG_MAP()

// Handler prototypes (uncomment arguments if needed):
//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

[!if WTL_USE_CPP_FILES]
	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
[!else]
	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	{
[!if WTL_COM_SERVER]
		_Module.Lock();

[!endif]
		// center the dialog on the screen
		CenterWindow();

		// set icons
		HICON hIcon = AtlLoadIconImage(IDR_MAINFRAME, LR_DEFAULTCOLOR, ::GetSystemMetrics(SM_CXICON), ::GetSystemMetrics(SM_CYICON));
		SetIcon(hIcon, TRUE);
		HICON hIconSmall = AtlLoadIconImage(IDR_MAINFRAME, LR_DEFAULTCOLOR, ::GetSystemMetrics(SM_CXSMICON), ::GetSystemMetrics(SM_CYSMICON));
		SetIcon(hIconSmall, FALSE);

		return TRUE;
	}

[!endif]
[!if WTL_COM_SERVER]
[!if WTL_USE_CPP_FILES]
	LRESULT OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
[!else]
	LRESULT OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	{
		// if UI is the last thread, no need to wait
		if(_Module.GetLockCount() == 1)
		{
			_Module.m_dwTimeOut = 0L;
			_Module.m_dwPause = 0L;
		}
		_Module.Unlock();
		return 0;
	}

[!endif]
[!endif]
[!if WTL_USE_CPP_FILES]
	LRESULT OnAppAbout(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
[!else]
	LRESULT OnAppAbout(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		CSimpleDialog<IDD_ABOUTBOX, FALSE> dlg;
		dlg.DoModal();
		return 0;
	}

[!endif]
[!if WTL_USE_CPP_FILES]
	LRESULT OnOK(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
[!else]
	LRESULT OnOK(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		// TODO: Add validation code 
		EndDialog(wID);
		return 0;
	}

[!endif]
[!if WTL_USE_CPP_FILES]
	LRESULT OnCancel(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
[!else]
	LRESULT OnCancel(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		EndDialog(wID);
		return 0;
	}
[!endif]
};
[!endif]
