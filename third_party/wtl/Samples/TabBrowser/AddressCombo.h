// AddressCombo.h - CAddressComboBox class

#pragma once

#define WM_COMBOMOUSEACTIVATE   (WM_APP + 10)


class CAddressComboBox : public CWindowImpl<CAddressComboBox, CComboBoxEx>
{
public:
	DECLARE_WND_SUPERCLASS(_T("TabBrowser_AddressComboBox"), CComboBoxEx::GetWndClassName())

	enum
	{
		m_cxGap = 2,
		m_cxMinDropWidth = 200
	};

	CComboBox m_cb;
	CToolBarCtrl m_tb;
	SIZE m_sizeTB;

	BEGIN_MSG_MAP(CAddressComboBox)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBkgnd)
		MESSAGE_HANDLER(WM_WINDOWPOSCHANGING, OnWindowPosChanging)
		NOTIFY_CODE_HANDLER(TTN_GETDISPINFO, OnToolTipText)
		MESSAGE_HANDLER(WM_MOUSEACTIVATE, OnMouseActivate)
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
			m_tb = CFrameWindowImplBase<>::CreateSimpleToolBarCtrl(m_hWnd, IDR_GO, FALSE, ATL_SIMPLE_TOOLBAR_PANE_STYLE | TBSTYLE_LIST);

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

		// position the GO button on the right
		m_tb.SetWindowPos(NULL, wp.cx + m_cxGap, y, m_sizeTB.cx, m_sizeTB.cy, SWP_SHOWWINDOW | SWP_NOACTIVATE | SWP_NOOWNERZORDER);

		return lRet;
	}

	LRESULT OnToolTipText(int idCtrl, LPNMHDR pnmh, BOOL& bHandled)
	{
		if(idCtrl == ID_GO)
			SendMessage(GetParent(), WM_NOTIFY, idCtrl, (LPARAM)pnmh);
		else
			bHandled = FALSE;

		return 0;
	}

	LRESULT OnMouseActivate(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
	{
		LRESULT lRet = DefWindowProc(uMsg, wParam, lParam);
		SendMessage(GetTopLevelParent(), WM_COMBOMOUSEACTIVATE, 0, 0L);

		return lRet;
	}
};
