// explorercombo.h

#ifndef __EXPLORERCOMBO_H__
#define __EXPLORERCOMBO_H__

#pragma once

#include "resource.h"


class CExplorerCombo : public CWindowImpl<CExplorerCombo, CComboBoxEx>
{
public:
	enum
	{
		m_cxGap = 2,
		m_cxMinDropWidth = 200
	};

	CComboBox m_cb;
	CToolBarCtrl m_tb;
	SIZE m_sizeTB;

	DECLARE_WND_SUPERCLASS(_T("WtlExplorer_ComboBox"), CComboBoxEx::GetWndClassName())

	BEGIN_MSG_MAP(CExplorerCombo)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBkgnd)
		MESSAGE_HANDLER(WM_WINDOWPOSCHANGING, OnWindowPosChanging)
	END_MSG_MAP()

	LRESULT OnCreate(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
	{
		// let the combo box initialize itself
		LRESULT lRet = DefWindowProc(uMsg, wParam, lParam);

		if(lRet != -1)
		{
			// adjust the drop-down width
			m_cb = GetComboCtrl();
			m_cb.SetDroppedWidth(m_cxMinDropWidth);

			// create a toolbar for the GO button
			m_tb = CFrameWindowImplBase<>::CreateSimpleToolBarCtrl(m_hWnd, IDT_GO1, FALSE, ATL_SIMPLE_TOOLBAR_PANE_STYLE | TBSTYLE_LIST);
			LPCTSTR lpszStrings = _T("Go\0");
			m_tb.AddStrings(lpszStrings);

			RECT rect;
			m_tb.GetItemRect(0, &rect);
			m_sizeTB.cx = rect.right;
			m_sizeTB.cy = rect.bottom;
		}

		return lRet;
	}

	LRESULT OnEraseBkgnd(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled)
	{
		CDCHandle dc = (HDC)wParam;
		CWindow wndParent = GetParent();

		// Forward this to the parent window, rebar bands are transparent
		POINT pt = { 0, 0 };
		MapWindowPoints(wndParent, &pt, 1);
		dc.OffsetWindowOrg(pt.x, pt.y, &pt);
		LRESULT lRet = wndParent.SendMessage(WM_ERASEBKGND, (WPARAM)dc.m_hDC);
		dc.SetWindowOrg(pt.x, pt.y);

		bHandled = (lRet != 0);
		return lRet;
	}

	LRESULT OnWindowPosChanging(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
	{
		if(m_tb.m_hWnd == NULL)
		{
			bHandled = FALSE;
			return 1;
		}

		// copy the WINDOWPOS struct and adjust for the GO button
		WINDOWPOS wp = *(LPWINDOWPOS)lParam;
		wp.cx -= m_sizeTB.cx + m_cxGap;
		LRESULT lRet = DefWindowProc(uMsg, wParam, (LPARAM)&wp);

		// paint below the GO button
		RECT rcGo = { wp.cx, 0, wp.cx + m_sizeTB.cx + m_cxGap, wp.cy };
		InvalidateRect(&rcGo);

		// center the GO button relative to the combo box
		RECT rcCombo;
		m_cb.GetWindowRect(&rcCombo);
		int y = (rcCombo.bottom - rcCombo.top - m_sizeTB.cy) / 2;
//		if(y < 0)
//			y = 0;

		// position the GO button on the right
		m_tb.SetWindowPos(NULL, wp.cx + m_cxGap, y, m_sizeTB.cx, m_sizeTB.cy, SWP_SHOWWINDOW | SWP_NOACTIVATE | SWP_NOOWNERZORDER);

		return lRet;
	}
};

#endif //__EXPLORERCOMBO_H__
