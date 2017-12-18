// aboutdlg.h : interface of the CAboutDlg class
//
/////////////////////////////////////////////////////////////////////////////

#pragma once

class CAboutDlg : public CAeroDialogImpl<CAboutDlg>
{
public:
	enum { IDD = IDD_ABOUTBOX };

	CAeroStatic m_Icon, m_Text;
	CAeroButton m_OK;

	BEGIN_MSG_MAP(CAboutDlg)
		CHAIN_MSG_MAP(CAeroDialogImpl<CAboutDlg>)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_ID_HANDLER(IDOK, OnCloseCmd)
		COMMAND_ID_HANDLER(IDCANCEL, OnCloseCmd)
	END_MSG_MAP()

// Handler prototypes (uncomment arguments if needed):
//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	{
		CenterWindow(GetParent());

		//MARGINS m = {7, 7, 7, 7};
		MARGINS m = {-1};
		SetMargins(m);

		m_Icon.SubclassWindow(GetDlgItem(IDC_APPICON));
		m_Text.SubclassWindow(GetDlgItem(IDC_TEXT));
		m_OK.SubclassWindow(GetDlgItem(IDOK));

		return TRUE;
	}

	LRESULT OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		EndDialog(wID);
		return 0;
	}
};
