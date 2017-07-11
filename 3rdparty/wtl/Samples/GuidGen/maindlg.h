// maindlg.h : interface of the CMainDlg class
//
/////////////////////////////////////////////////////////////////////////////

#if !defined(AFX_MAINDLG_H__6920296A_4C3F_11D1_AA9A_000000000000__INCLUDED_)
#define AFX_MAINDLG_H__6920296A_4C3F_11D1_AA9A_000000000000__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

LPCTSTR _lpstrRegKey = _T("Software\\Microsoft\\WTL Samples\\GUIDGEN");

class CMainDlg : public CDialogImpl<CMainDlg>, public CMessageFilter
{
public:
	enum { IDD = IDD_GUIDGEN_DIALOG };

	int m_nGuidType;
	GUID m_guid;

	CMainDlg() : m_nGuidType(0)
	{
	}

	virtual BOOL PreTranslateMessage(MSG* pMsg)
	{
		return ::IsDialogMessage(m_hWnd, pMsg);
	}

	void UpdateData()
	{
		m_nGuidType = 0;
		m_nGuidType = IsDlgButtonChecked(IDC_RADIO2) ? 1 : m_nGuidType;
		m_nGuidType = IsDlgButtonChecked(IDC_RADIO3) ? 2 : m_nGuidType;
		m_nGuidType = IsDlgButtonChecked(IDC_RADIO4) ? 3 : m_nGuidType;
		_ASSERTE(m_nGuidType >= 0 && m_nGuidType <= 3);
	}

	BEGIN_MSG_MAP(CMainDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_ID_HANDLER(IDOK, OnOK)
		COMMAND_ID_HANDLER(IDCANCEL, OnCancel)
		COMMAND_ID_HANDLER(IDC_NEWGUID, OnNewGUID)
		COMMAND_RANGE_HANDLER(IDC_RADIO1, IDC_RADIO4, OnSelChange)
		MESSAGE_HANDLER(WM_SYSCOMMAND, OnSysCommand)
	END_MSG_MAP()

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	{
		// center the dialog on the screen
		CenterWindow();

		// set icons
		HICON hIcon = (HICON)::LoadImage(_Module.GetResourceInstance(), MAKEINTRESOURCE(IDR_MAINFRAME), 
			IMAGE_ICON, ::GetSystemMetrics(SM_CXICON), ::GetSystemMetrics(SM_CYICON), LR_DEFAULTCOLOR);
		SetIcon(hIcon, TRUE);
		HICON hIconSmall = (HICON)::LoadImage(_Module.GetResourceInstance(), MAKEINTRESOURCE(IDR_MAINFRAME), 
			IMAGE_ICON, ::GetSystemMetrics(SM_CXSMICON), ::GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);
		SetIcon(hIconSmall, FALSE);

		// Add "About..." menu item to system menu.

		// IDM_ABOUTBOX must be in the system command range.
		_ASSERTE((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
		_ASSERTE(IDM_ABOUTBOX < 0xF000);

		CMenu SysMenu = GetSystemMenu(FALSE);
		if(::IsMenu(SysMenu))
		{
			TCHAR szAboutMenu[256];
			if(::LoadString(_Module.GetResourceInstance(), IDS_ABOUTBOX, szAboutMenu, 255) > 0)
			{
				SysMenu.AppendMenu(MF_SEPARATOR);
				SysMenu.AppendMenu(MF_STRING, IDM_ABOUTBOX, szAboutMenu);
			}
		}
		SysMenu.Detach();

		// register object for message filtering
		CMessageLoop* pLoop = _Module.GetMessageLoop();
		pLoop->AddMessageFilter(this);

		CRegKeyEx reg;
		long lRet = reg.Open(HKEY_CURRENT_USER, _lpstrRegKey, KEY_READ);
		if(lRet == ERROR_SUCCESS)
		{
			DWORD dwVal = 0;
			lRet = reg.QueryDWORDValue(_T("GUID Type"), dwVal);
			if(lRet == ERROR_SUCCESS)
				m_nGuidType = (int)dwVal;
		}
		CheckRadioButton(IDC_RADIO1, IDC_RADIO4, IDC_RADIO1 + m_nGuidType);

		if(!NewGUID())
			CloseDialog(IDABORT);

		DisplayGUID();

		return TRUE;
	}

	void GetFormattedGuid(TCHAR* rString)
	{
		// load appropriate formatting string
		TCHAR szBuf[256];
		::LoadString(_Module.GetResourceInstance(), IDS_FORMATS+m_nGuidType, szBuf, 255);
		wsprintf(rString, szBuf, 
			// first copy...
			m_guid.Data1, m_guid.Data2, m_guid.Data3, 
			m_guid.Data4[0], m_guid.Data4[1], m_guid.Data4[2], m_guid.Data4[3],
			m_guid.Data4[4], m_guid.Data4[5], m_guid.Data4[6], m_guid.Data4[7],
			// second copy...
			m_guid.Data1, m_guid.Data2, m_guid.Data3, 
			m_guid.Data4[0], m_guid.Data4[1], m_guid.Data4[2], m_guid.Data4[3],
			m_guid.Data4[4], m_guid.Data4[5], m_guid.Data4[6], m_guid.Data4[7]);
	}

	void DisplayGUID()
	{
		TCHAR szBuf[512];
		GetFormattedGuid(szBuf);
		SetDlgItemText(IDC_RESULTS, szBuf);
	}

	BOOL NewGUID()
	{
		m_guid = GUID_NULL;
		::CoCreateGuid(&m_guid);
		if(m_guid == GUID_NULL)
		{
			TCHAR szBuf[256];
			::LoadString(_Module.GetResourceInstance(), IDP_ERR_CREATE_GUID, szBuf, 255);
			MessageBox(szBuf, _T("GUIDGen"), MB_OK);
			return FALSE;
		}
		return TRUE;
	}

	LRESULT OnNewGUID(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		if(!NewGUID())
			return 0;
		DisplayGUID();
		return 0;
	}
	
	LRESULT OnSelChange(WORD wNotifyCode, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		if(wNotifyCode == BN_CLICKED)
		{
			UpdateData();
			DisplayGUID();
		}
		return 0;
	}

	LRESULT OnOK(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		UpdateData();
		if(!OpenClipboard())
		{
			TCHAR szBuf[256];
			::LoadString(_Module.GetResourceInstance(), IDP_ERR_OPEN_CLIP, szBuf, 255);
			MessageBox(szBuf, _T("GUIDGen"), MB_OK);
			return 0;
		}

		TCHAR strResult[512];
		GetFormattedGuid(strResult);
		int nTextLen = (lstrlen(strResult) + 1) * sizeof(TCHAR);
		HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, nTextLen);
		if(hGlobal != NULL)
		{
			LPVOID lpText = GlobalLock(hGlobal);
			memcpy(lpText, strResult, nTextLen);

			EmptyClipboard();
			GlobalUnlock(hGlobal);
#ifdef _UNICODE
			SetClipboardData(CF_UNICODETEXT, hGlobal);
#else
			SetClipboardData(CF_TEXT, hGlobal);
#endif
		}
		CloseClipboard();

		return 0;
	}

	LRESULT OnCancel(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		CRegKeyEx reg;
		long lRet = reg.Create(HKEY_CURRENT_USER, _lpstrRegKey);
		if(lRet == ERROR_SUCCESS)
		{
//			DWORD dwVal = m_nGuidType;
//			reg.SetDWORDValue(_T("GUID Type"), dwVal);
			reg.SetDWORDValue(_T("GUID Type"), m_nGuidType);
		}

		CloseDialog(wID);
		return 0;
	}

	void CloseDialog(int nVal)
	{
		DestroyWindow();
		::PostQuitMessage(nVal);
	}

	LRESULT OnSysCommand(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled)
	{
		UINT uCmdType = (UINT)wParam;

		if((uCmdType & 0xFFF0) == IDM_ABOUTBOX)
		{
			CAboutDlg dlg;
			dlg.DoModal();
		}
		else
			bHandled = FALSE;

		return 0;
	}
};


/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_MAINDLG_H__6920296A_4C3F_11D1_AA9A_000000000000__INCLUDED_)
