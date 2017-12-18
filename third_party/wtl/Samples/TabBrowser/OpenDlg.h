// OpenDlg.h - COpenDlg class

#pragma once


class COpenDlg : public CDialogImpl<COpenDlg>
{
public:
	enum { IDD = IDD_OPEN };

	CString m_strURL;
	bool m_bNewTab;

	COpenDlg(LPCTSTR lpstrURL) : m_bNewTab(false)
	{
		m_strURL = lpstrURL;
	}

	BEGIN_MSG_MAP(COpenDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_ID_HANDLER(IDOK, OnOK)
		COMMAND_ID_HANDLER(IDCANCEL, OnCancel)
	END_MSG_MAP()

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	{

		if(m_strURL.IsEmpty())
		{
			m_bNewTab = true;
			CButton btnCheck = GetDlgItem(IDC_NEW_TAB);
			btnCheck.SetCheck(1);
			btnCheck.EnableWindow(FALSE);
		}
		else
		{
			CEdit edit = GetDlgItem(IDC_EDIT_URL);
			edit.SetWindowText(m_strURL);

			CButton btnCheck = GetDlgItem(IDC_NEW_TAB);
			btnCheck.SetCheck(m_bNewTab ? 1 : 0);
		}

		return TRUE;
	}

	LRESULT OnOK(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		CEdit edit = GetDlgItem(IDC_EDIT_URL);
		int nLen = edit.GetWindowTextLength();
		edit.GetWindowText(m_strURL.GetBuffer(nLen), nLen + 1);
		m_strURL.ReleaseBuffer();

		CButton btnCheck = GetDlgItem(IDC_NEW_TAB);
		m_bNewTab = (btnCheck.GetCheck() != 0);

		EndDialog(wID);
		return 0;
	}

	LRESULT OnCancel(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		EndDialog(wID);
		return 0;
	}
};
