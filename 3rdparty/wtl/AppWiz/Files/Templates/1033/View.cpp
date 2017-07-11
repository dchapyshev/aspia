// [!output WTL_VIEW_FILE].cpp : implementation of the [!output WTL_VIEW_CLASS] class
//
/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
[!if WTL_USE_RIBBON]
#include "Ribbon.h"
[!endif]
#include "resource.h"

#include "[!output WTL_VIEW_FILE].h"

BOOL [!output WTL_VIEW_CLASS]::PreTranslateMessage(MSG* pMsg)
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
[!if WTL_VIEWTYPE_SCROLL]

void [!output WTL_VIEW_CLASS]::DoPaint(CDCHandle dc)
{
	//TODO: Add your drawing code here
}
[!endif]
[!if WTL_APPTYPE_TABVIEW]

void [!output WTL_VIEW_CLASS]::OnFinalMessage(HWND /*hWnd*/)
{
	delete this;
}
[!endif]
[!if WTL_VIEWTYPE_GENERIC]

LRESULT [!output WTL_VIEW_CLASS]::OnPaint(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	CPaintDC dc(m_hWnd);

	//TODO: Add your drawing code here

	return 0;
}
[!endif]
