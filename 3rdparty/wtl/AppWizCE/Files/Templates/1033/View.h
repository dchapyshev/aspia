// [!output WTL_VIEW_FILE].h : interface of the [!output WTL_VIEW_CLASS] class
//
/////////////////////////////////////////////////////////////////////////////

#pragma once

[!if WTL_VIEWTYPE_GENERIC || WTL_VIEWTYPE_FORM]
class [!output WTL_VIEW_CLASS] : public [!output WTL_VIEW_BASE_CLASS]<[!output WTL_VIEW_CLASS]>
[!else]
class [!output WTL_VIEW_CLASS] : public [!output WTL_VIEW_BASE_CLASS]<[!output WTL_VIEW_CLASS], [!output WTL_VIEW_BASE]>
[!endif]
{
public:
[!if WTL_VIEWTYPE_GENERIC]
	DECLARE_WND_CLASS(NULL)
[!else]
[!if WTL_VIEWTYPE_FORM]
	enum { IDD = IDD_[!output UPPERCASE_SAFE_PROJECT_NAME]_FORM };
[!else]
	DECLARE_WND_SUPERCLASS(NULL, [!output WTL_VIEW_BASE]::GetWndClassName())
[!endif]
[!endif]
[!if WTL_USE_CPP_FILES]

	BOOL PreTranslateMessage(MSG* pMsg);
[!else]

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
[!if WTL_VIEWTYPE_HTML]
		if((pMsg->message < WM_KEYFIRST || pMsg->message > WM_KEYLAST) &&
		   (pMsg->message < WM_MOUSEFIRST || pMsg->message > WM_MOUSELAST))
			return FALSE;

		// give HTML page a chance to translate this message
		return (BOOL)SendMessage(WM_FORWARDMSG, 0, (LPARAM)pMsg);
[!else]
[!if WTL_VIEWTYPE_FORM]
		return CWindow::IsDialogMessage(pMsg);
[!else]
		pMsg;
		return FALSE;
[!endif]
[!endif]
	}
[!endif]

	BEGIN_MSG_MAP([!output WTL_VIEW_CLASS])
[!if WTL_VIEWTYPE_GENERIC]
		MESSAGE_HANDLER(WM_PAINT, OnPaint)
[!endif]
	END_MSG_MAP()

// Handler prototypes (uncomment arguments if needed):
//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)
[!if WTL_VIEWTYPE_GENERIC]
[!if WTL_USE_CPP_FILES]

	LRESULT OnPaint(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
[!else]

	LRESULT OnPaint(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	{
		CPaintDC dc(m_hWnd);

		//TODO: Add your drawing code here

		return 0;
	}
[!endif]
[!endif]
};
