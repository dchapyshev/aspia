class CAboutDlg : public CDialogImpl<CAboutDlg>
{
public:
	enum { IDD = IDD_ABOUTDLG };

	CFont m_font;

	BEGIN_MSG_MAP(CAboutDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_ID_HANDLER(IDOK, OnCloseCmd)
		COMMAND_ID_HANDLER(IDCANCEL, OnCloseCmd)
	END_MSG_MAP()

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	{
		CenterWindow(GetParent());

		// Set Tahoma font if we are not running on Vista or higher
		if(!RunTimeHelper::IsVista())
		{
			CLogFont lf;
			lf.lfWeight = FW_NORMAL;
			lf.SetHeight(8);
			SecureHelper::strcpy_x(lf.lfFaceName, LF_FACESIZE, _T("Tahoma"));
			m_font.CreateFontIndirect(&lf);

			SetFont(m_font);

			for(CWindow wnd = GetWindow(GW_CHILD); wnd.m_hWnd != NULL; wnd = wnd.GetWindow(GW_HWNDNEXT))
				wnd.SetFont(m_font);
		}

		return (LRESULT)TRUE;
	}

	LRESULT OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		EndDialog(wID);
		return 0;
	}
};
