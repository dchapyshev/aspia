// WindowsDlg.h - CWindowsDlg class

#pragma once


class CWindowsDlg : public CDialogImpl<CWindowsDlg>, public CDialogResize<CWindowsDlg>
{
public:
	enum { IDD = IDD_WINDOWS };

	CCustomTabView* m_pTabView;
	CListBox m_lb;

	CWindowsDlg(CCustomTabView* pTabView) : m_pTabView(pTabView)
	{ }

	void UpdateUI()
	{
		CButton btnActivate = GetDlgItem(IDC_ACTIVATE);
		btnActivate.EnableWindow((m_lb.GetSelCount() == 1) ? TRUE : FALSE);

		CButton btnClose = GetDlgItem(IDC_CLOSEWINDOWS);
		btnClose.EnableWindow((m_lb.GetSelCount() != 0) ? TRUE : FALSE);
	}

	BEGIN_DLGRESIZE_MAP(CWindowsDlg)
		DLGRESIZE_CONTROL(IDC_LIST1, DLSZ_SIZE_X | DLSZ_SIZE_Y)
		DLGRESIZE_CONTROL(IDC_ACTIVATE, DLSZ_MOVE_X)
		DLGRESIZE_CONTROL(IDC_CLOSEWINDOWS, DLSZ_MOVE_X)
		DLGRESIZE_CONTROL(IDOK, DLSZ_MOVE_X | DLSZ_MOVE_Y)
	END_DLGRESIZE_MAP()

	BEGIN_MSG_MAP(CWindowsDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_HANDLER(IDC_LIST1, LBN_SELCHANGE, OnSelChange)
		COMMAND_ID_HANDLER(IDC_ACTIVATE, OnActivate)
		COMMAND_HANDLER(IDC_LIST1, LBN_DBLCLK, OnActivate)
		COMMAND_ID_HANDLER(IDC_CLOSEWINDOWS, OnCloseWindows)
		COMMAND_ID_HANDLER(IDOK, OnCloseCmd)
		COMMAND_ID_HANDLER(IDCANCEL, OnCloseCmd)
		CHAIN_MSG_MAP(CDialogResize<CWindowsDlg>)
	END_MSG_MAP()

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	{
		CenterWindow(GetParent());

		// Fill list box
		m_lb = GetDlgItem(IDC_LIST1);
		for(int i = 0; i < m_pTabView->GetPageCount(); i++)
			m_lb.AddString(m_pTabView->GetPageTitle(i));

		m_lb.SetSel(m_pTabView->GetActivePage());

		DlgResize_Init();

		m_ptMinTrackSize.x /= 2;
		m_ptMinTrackSize.y /= 2;

		UpdateUI();

		return TRUE;
	}

	LRESULT OnSelChange(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		UpdateUI();
		return 0;
	}

	LRESULT OnActivate(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		int nSel = -1;
		for(int i = 0; i < m_lb.GetCount(); i++)
		{
			if(m_lb.GetSel(i) > 0)
			{
				nSel = i;
				break;
			}
		}

		if(nSel != -1)
		{
			m_pTabView->SetActivePage(nSel);
			EndDialog(IDOK);
		}

		return 0;
	}

	LRESULT OnCloseWindows(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		int nCount = m_lb.GetCount();
		int nLast = -1;
		for(int i = nCount - 1; i >= 0; i--)
		{
			if(m_lb.GetSel(i) > 0)
			{
				m_pTabView->RemovePage(i);
				m_lb.DeleteString(i);
				nLast = i;
			}
		}

		if(nLast != -1)
			m_lb.SetSel(nLast);

		return 0;
	}

	LRESULT OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		EndDialog(wID);
		return 0;
	}
};
