// [!output WTL_CHILD_FRAME_FILE].h : interface of the [!output WTL_CHILD_FRAME_CLASS] class
//
/////////////////////////////////////////////////////////////////////////////

#pragma once

class [!output WTL_CHILD_FRAME_CLASS] : public [!output WTL_CHILD_FRAME_BASE_CLASS]<[!output WTL_CHILD_FRAME_CLASS]>
{
public:
	DECLARE_FRAME_WND_CLASS(NULL, IDR_MDICHILD)

[!if WTL_USE_VIEW]
	[!output WTL_VIEW_CLASS] m_view;

[!endif]
[!if WTL_VIEWTYPE_LISTBOX || WTL_VIEWTYPE_EDIT || WTL_VIEWTYPE_RICHEDIT]
	CFont m_font;

[!endif]
[!if WTL_USE_CPP_FILES]
	virtual void OnFinalMessage(HWND /*hWnd*/);
[!else]
	virtual void OnFinalMessage(HWND /*hWnd*/)
	{
		delete this;
	}
[!endif]

	BEGIN_MSG_MAP([!output WTL_CHILD_FRAME_CLASS])
[!if WTL_USE_VIEW]
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
[!endif]
		MESSAGE_HANDLER(WM_FORWARDMSG, OnForwardMsg)
		CHAIN_MSG_MAP([!output WTL_CHILD_FRAME_BASE_CLASS]<[!output WTL_CHILD_FRAME_CLASS]>)
	END_MSG_MAP()

// Handler prototypes (uncomment arguments if needed):
//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

[!if WTL_USE_VIEW]
[!if WTL_USE_CPP_FILES]
	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
[!else]
	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
	{
[!if WTL_VIEWTYPE_FORM]
		m_hWndClient = m_view.Create(m_hWnd);
[!else]
[!if WTL_VIEWTYPE_HTML]
		//TODO: Replace with a URL of your choice
		m_hWndClient = m_view.Create(m_hWnd, rcDefault, _T("http://www.microsoft.com"), [!output WTL_VIEW_STYLES], [!output WTL_VIEW_EX_STYLES]);
[!else]
		m_hWndClient = m_view.Create(m_hWnd, rcDefault, NULL, [!output WTL_VIEW_STYLES], [!output WTL_VIEW_EX_STYLES]);
[!if WTL_VIEWTYPE_LISTBOX || WTL_VIEWTYPE_EDIT || WTL_VIEWTYPE_RICHEDIT]
		m_font = AtlCreateControlFont();
		m_view.SetFont(m_font);
[!endif]
[!if WTL_VIEWTYPE_SCROLL]
		// replace with appropriate values for the app
		m_view.SetScrollSize(2000, 1000);
[!endif]
[!endif]
[!endif]

		bHandled = FALSE;
		return 1;
	}

[!endif]
[!endif]
[!if WTL_USE_CPP_FILES]
	LRESULT OnForwardMsg(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/);
[!else]
	LRESULT OnForwardMsg(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/)
	{
		LPMSG pMsg = (LPMSG)lParam;

[!if WTL_USE_VIEW]
		if([!output WTL_CHILD_FRAME_BASE_CLASS]<[!output WTL_CHILD_FRAME_CLASS]>::PreTranslateMessage(pMsg))
			return TRUE;

		return m_view.PreTranslateMessage(pMsg);
[!else]
		return [!output WTL_CHILD_FRAME_BASE_CLASS]<[!output WTL_CHILD_FRAME_CLASS]>::PreTranslateMessage(pMsg);
[!endif]
	}
[!endif]
};
