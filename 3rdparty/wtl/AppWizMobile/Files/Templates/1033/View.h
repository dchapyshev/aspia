// [!output WTL_VIEW_FILE].h : interface of the [!output WTL_VIEW_CLASS] class
//
/////////////////////////////////////////////////////////////////////////////

#pragma once

[!if WTL_VIEWTYPE_GENERIC || WTL_VIEWTYPE_FORM || WTL_VIEWTYPE_PROPSHEET]
class [!output WTL_VIEW_CLASS] : 
[!if WTL_VIEW_SCROLL]
	public [!output WTL_VIEW_BASE_CLASS]<[!output WTL_VIEW_CLASS]>, 
	public [!output WTL_SCROLL_CLASS]<[!output WTL_VIEW_CLASS]>
[!else]
	public [!output WTL_VIEW_BASE_CLASS]<[!output WTL_VIEW_CLASS]>
[!endif]
[!else]
class [!output WTL_VIEW_CLASS] : 
	public [!output WTL_VIEW_BASE_CLASS]<[!output WTL_VIEW_CLASS], [!output WTL_VIEW_BASE]>
[!endif]
{
public:
[!if WTL_VIEWTYPE_GENERIC || WTL_VIEWTYPE_PROPSHEET]
	DECLARE_WND_CLASS(NULL)
[!else]
[!if WTL_VIEWTYPE_FORM]
	enum { IDD = IDD_[!output UPPERCASE_SAFE_PROJECT_NAME]_FORM };
[!else]
	DECLARE_WND_SUPERCLASS(NULL, [!output WTL_VIEW_BASE]::GetWndClassName())
[!endif]
[!endif]

	BOOL PreTranslateMessage(MSG* pMsg)
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
[!if WTL_VIEWTYPE_FORM || WTL_VIEWTYPE_PROPSHEET]
		return IsDialogMessage(pMsg);
[!else]
[!if !WTL_HOST_AX]
		pMsg;
[!endif]
		return FALSE;
[!endif]
	}
[!if WTL_VIEW_SCROLL]

	void DoPaint(CDCHandle dc)
	{
		//TODO: Add your drawing code here

	}
[!endif]
[!if WTL_VIEWTYPE_PROPSHEET]

public:
	CPropertyPage<IDD_[!output UPPERCASE_SAFE_PROJECT_NAME]_PAGE> m_Page1, m_Page2;

	[!output WTL_VIEW_CLASS]()
	{
		SetTitle(_T("Properties"));
		SetLinkText(_T("Tap <file:\\Windows\\default.htm{here}>."));
		m_Page1.SetTitle(_T("Page 1"));
		AddPage(m_Page1);
		m_Page2.SetTitle(_T("Page 2"));
		AddPage(m_Page2);
	}

[!else]

	BEGIN_MSG_MAP([!output WTL_VIEW_CLASS])
[!if WTL_VIEWTYPE_GENERIC]
[!if WTL_VIEW_SCROLL]
		CHAIN_MSG_MAP([!output WTL_SCROLL_CLASS]<[!output WTL_VIEW_CLASS]>)
[!else]
		MESSAGE_HANDLER(WM_PAINT, OnPaint)
[!endif]
[!endif]
	END_MSG_MAP()

// Handler prototypes (uncomment arguments if needed):
//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)
[!if WTL_VIEWTYPE_GENERIC && !WTL_VIEW_SCROLL]

	LRESULT OnPaint(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	{
		CPaintDC dc(m_hWnd);

		//TODO: Add your drawing code here

		return 0;
	}
[!endif]
[!endif]
};

