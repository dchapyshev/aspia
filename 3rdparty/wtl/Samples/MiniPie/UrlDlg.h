// UrlDlg.h : interface of the CUrlDlg class
//
/////////////////////////////////////////////////////////////////////////////

#pragma once

class CUrlDlg : 
	public CStdDialogResizeImpl<CUrlDlg>,	
	public CWinDataExchange<CUrlDlg>
{
public:
	class CEditUrl : // a CEdit handling VK_RETURN and Edit commands
		public CWindowImpl<CEditUrl, CEdit>, public CEditCommands<CEditUrl>
	{
	public:
		DECLARE_WND_SUPERCLASS(L"MiniPie.EditUrl", CEdit::GetWndClassName())

		BEGIN_MSG_MAP(CEditUrl)
			MESSAGE_RANGE_HANDLER(WM_KEYFIRST, WM_KEYLAST, OnKey)
		ALT_MSG_MAP(1)
			CHAIN_MSG_MAP_ALT(CEditCommands<CEditUrl>, 1)
		END_MSG_MAP()

		LRESULT OnKey(UINT uMsg, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled);
	};

	enum { IDD = IDD_URL };
	static const LPCTSTR m_sTypes[3];

#ifdef WIN32_PLATFORM_PSPC
	CListBox m_lbType;
#else
	CSpinListBox m_lbType;
#endif
	CEditUrl m_Ed;
	int m_iType;
	CString m_strUrl;

	LPCTSTR GetUrl(void);
	void StdCloseDialog(WORD wID);

	BEGIN_DLGRESIZE_MAP(CUrlDlg)
		DLGRESIZE_CONTROL(IDC_EDIT_URL, DLSZ_SIZE_X | DLSZ_SIZE_Y)	
	END_DLGRESIZE_MAP()

	BEGIN_DDX_MAP(CUrlDlg)
		DDX_CONTROL_HANDLE(IDC_TYPE_LIST, m_lbType)
		DDX_CONTROL(IDC_EDIT_URL, m_Ed)
		DDX_TEXT(IDC_EDIT_URL, m_strUrl)
	END_DDX_MAP()

    BEGIN_MSG_MAP(CUrlDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
#ifdef WIN32_PLATFORM_PSPC
		MESSAGE_HANDLER(WM_SETTINGCHANGE, OnSettingChange)
#endif
		CHAIN_COMMANDS_ALT_MEMBER(m_Ed, 1)
		CHAIN_MSG_MAP(CStdDialogResizeImpl<CUrlDlg>)
    END_MSG_MAP()

	LRESULT OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
};


