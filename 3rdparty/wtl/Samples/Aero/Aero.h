// Aero.h
//
//  Aero sample support classes

#ifndef __AERO_H__
#define __AERO_H__

#pragma once

#if !defined _WTL_VER || _WTL_VER < 0x800
	#error Aero.h requires the Windows Template Library 8.0
#endif

#if _WIN32_WINNT < 0x0600
	#error Aero.h requires _WIN32_WINNT >= 0x0600
#elif !defined(NTDDI_VERSION) || (NTDDI_VERSION < NTDDI_LONGHORN)
	#error Aero.h requires the Windows Vista SDK
#endif

#ifndef __ATLTHEME_H__
	#include "atltheme.h"
#endif

#include <dwmapi.h>
#pragma comment (lib, "dwmapi.lib")

#if (_MSC_VER < 1300) && !defined(_WTL_NO_THEME_DELAYLOAD)
  #pragma comment(lib, "delayimp.lib")
  #pragma comment(linker, "/delayload:dwmapi.dll")
#endif // (_MSC_VER < 1300) && !defined(_WTL_NO_THEME_DELAYLOAD)


///////////////////////////////////////////////////////////////////////////////
// Classes in this file:
//
// CAeroImpl<T> - enables Aero translucency (when available) for any window
// CAeroDialogImpl<T, TBase> - dialog implementation of Aero translucency (when available)
// CAeroFrameImpl<T, TBase, TWinTraits> - frame implementation of Aero translucency (when available) 
// CAeroCtrlImpl - base implementation of Aero opacity for controls
// CAeroStatic - Aero opaque Static control
// CAeroButton - Aero opaque Button control
// CAeroEdit - Aero opaque Edit control

namespace WTL
{

template <class T>
class CAeroImpl :
	public CBufferedPaintImpl<T>,
	public CThemeImpl<T>
{
public:
	CAeroImpl(LPCWSTR lpstrThemeClassList = L"globals") 
	{
		m_PaintParams.dwFlags = BPPF_ERASE;

		SetThemeClassList(lpstrThemeClassList);
		MARGINS m = {0};
		m_Margins = m;
	}

	static bool IsAeroSupported() 
	{
		return IsBufferedPaintSupported();
	}

	bool IsCompositionEnabled() const
	{
		BOOL bEnabled = FALSE;
		return IsAeroSupported() ? SUCCEEDED(DwmIsCompositionEnabled(&bEnabled)) && bEnabled : false;
	}    
	
	bool IsTheming() const
	{
		return m_hTheme != 0;
	}    
	
	MARGINS m_Margins;

	bool SetMargins(MARGINS& m)
	{
		m_Margins = m;
		T* pT = static_cast<T*>(this);
		return pT->IsWindow() && IsAeroSupported() ? SUCCEEDED(DwmExtendFrameIntoClientArea(pT->m_hWnd, &m_Margins)) : true;
	}

// implementation
	void DoPaint(CDCHandle dc, RECT& rDest)
	{
		T* pT = static_cast<T*>(this);

		RECT rClient;
		pT->GetClientRect(&rClient);

		RECT rView = {rClient.left + m_Margins.cxLeftWidth, rClient.top + m_Margins.cyTopHeight, 
			rClient.right - m_Margins.cxRightWidth, rClient.bottom - m_Margins.cyBottomHeight};

		if (IsCompositionEnabled())
				dc.FillSolidRect(&rClient, RGB(0, 0, 0));
		else
			if (IsTheming())
				pT->DrawThemeBackground(dc, WP_FRAMEBOTTOM, pT->m_hWnd == GetFocus() ? 1 : 2, &rClient, &rDest);
			else
				dc.FillSolidRect(&rClient, GetSysColor(COLOR_MENUBAR));

		if (m_Margins.cxLeftWidth != -1)
			dc.FillSolidRect(&rView, GetSysColor(COLOR_WINDOW));
		else 
			::SetRectEmpty(&rView);

		pT->AeroDoPaint(dc, rClient, rView, rDest);
	}

	void AeroDrawText(HDC dc, LPCTSTR pStr, LPRECT prText, UINT uFormat, DTTOPTS &dto)
	{
        if(IsTheming())
			if (IsAeroSupported())
				DrawThemeTextEx (dc, TEXT_BODYTITLE, 0, pStr, -1, uFormat, prText, &dto );
			else
				DrawThemeText(dc, TEXT_BODYTITLE, 0, pStr, -1, uFormat, 0, prText);
		else
			CDCHandle(dc).DrawText(pStr, -1, prText, uFormat);
	}

	void AeroDrawText(HDC dc, LPCTSTR pStr, LPRECT prText, UINT uFormat, DWORD dwFlags, int iGlowSize)
	{
		DTTOPTS dto = { sizeof(DTTOPTS) };
		dto.dwFlags = dwFlags;
		dto.iGlowSize = iGlowSize;
		AeroDrawText(dc, pStr, prText, uFormat, dto);
	}

// overridable
	void AeroDoPaint(CDCHandle dc, RECT& rClient, RECT& rView, RECT& rDest)
	{}

	BEGIN_MSG_MAP(CAeroImpl)
		CHAIN_MSG_MAP(CThemeImpl<T>)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		MESSAGE_HANDLER(WM_ACTIVATE, OnActivate)
        MESSAGE_HANDLER(WM_DWMCOMPOSITIONCHANGED, OnCompositionChanged)
		MESSAGE_HANDLER(WM_PRINTCLIENT, OnPrintClient)
		CHAIN_MSG_MAP(CBufferedPaintImpl<T>)
	END_MSG_MAP()

	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
	{
		if (IsThemingSupported())
			OpenThemeData();

		if (IsCompositionEnabled())
			DwmExtendFrameIntoClientArea(static_cast<T*>(this)->m_hWnd, &m_Margins);
		return bHandled = FALSE;
	}

	LRESULT OnActivate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
	{
		if (!IsCompositionEnabled() && IsTheming())
			static_cast<T*>(this)->Invalidate(FALSE);
		return bHandled = FALSE;
	}

	LRESULT OnCompositionChanged(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	{
		if (IsCompositionEnabled())
			SetMargins(m_Margins);
		return 0;
	}

	LRESULT OnPrintClient(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
	{
		T* pT = static_cast<T*>(this);
		return ::DefWindowProc(pT->m_hWnd, uMsg, wParam, lParam);
	}
};


///////////////////////////////////////////////////////////////////////////////
// CAeroDialogImpl - dialog implementation of Aero translucency (when available)

template <class T, class TBase  = CWindow>
class ATL_NO_VTABLE CAeroDialogImpl : public CDialogImpl<T, TBase>, public CAeroImpl<T>
{
public:
	CAeroDialogImpl(LPCWSTR lpstrThemeClassList = L"dialog") : CAeroImpl(lpstrThemeClassList)
	{}

	void AeroDoPaint(CDCHandle dc, RECT& rClient, RECT& rView, RECT& rDest)
	{
		if (!::IsRectEmpty(&rView))
		{
			if (IsTheming())
				DrawThemeBackground(dc, WP_DIALOG, 1, &rView, &rDest);
			else
				dc.FillSolidRect(&rView, GetSysColor(COLOR_BTNFACE));
		}
	}

	BEGIN_MSG_MAP(CAeroDialogImpl)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		CHAIN_MSG_MAP(CAeroImpl<T>)
	END_MSG_MAP()

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
	{
		if (IsThemingSupported())
		{
			OpenThemeData();
			EnableThemeDialogTexture(ETDT_ENABLE);
		}

		if (IsCompositionEnabled())
			DwmExtendFrameIntoClientArea(static_cast<T*>(this)->m_hWnd, &m_Margins);

		return bHandled = FALSE;
	}

};

///////////////////////////////////////////////////////////////////////////////
// CAeroFrameImpl - frame implementation of Aero translucency (when available)

template <class T, class TBase = ATL::CWindow, class TWinTraits = ATL::CFrameWinTraits>
class ATL_NO_VTABLE CAeroFrameImpl : 
	public CFrameWindowImpl<T, TBase, TWinTraits>,
	public CAeroImpl<T>
{
	typedef CFrameWindowImpl<T, TBase, TWinTraits> _baseClass;
public:
	CAeroFrameImpl(LPCWSTR lpstrThemeClassList = L"window") : CAeroImpl(lpstrThemeClassList)
	{}

	void UpdateLayout(BOOL bResizeBars = TRUE)
	{
		RECT rect = { 0 };
		GetClientRect(&rect);

		// position margins
		if (m_Margins.cxLeftWidth != -1)
		{
			rect.left += m_Margins.cxLeftWidth;
			rect.top += m_Margins.cyTopHeight;
			rect.right -= m_Margins.cxRightWidth;
			rect.bottom -= m_Margins.cyBottomHeight;
		}

		// position bars and offset their dimensions
		UpdateBarsPosition(rect, bResizeBars);

		// resize client window
		if(m_hWndClient != NULL)
			::SetWindowPos(m_hWndClient, NULL, rect.left, rect.top,
				rect.right - rect.left, rect.bottom - rect.top,
				SWP_NOZORDER | SWP_NOACTIVATE);

		Invalidate(FALSE);
	}

	void UpdateBarsPosition(RECT& rect, BOOL bResizeBars = TRUE)
	{
		// resize toolbar
		if(m_hWndToolBar != NULL && ((DWORD)::GetWindowLong(m_hWndToolBar, GWL_STYLE) & WS_VISIBLE))
		{
			RECT rectTB = { 0 };
			::GetWindowRect(m_hWndToolBar, &rectTB);
			if(bResizeBars)
			{
				::SetWindowPos(m_hWndToolBar, NULL, rect.left, rect.top,
					rect.right - rect.left, rectTB.bottom - rectTB.top,
					SWP_NOZORDER | SWP_NOACTIVATE);
				::InvalidateRect(m_hWndToolBar, NULL, FALSE);
			}
			rect.top += rectTB.bottom - rectTB.top;
		}

		// resize status bar
		if(m_hWndStatusBar != NULL && ((DWORD)::GetWindowLong(m_hWndStatusBar, GWL_STYLE) & WS_VISIBLE))
		{
			RECT rectSB = { 0 };
			::GetWindowRect(m_hWndStatusBar, &rectSB);
			rect.bottom -= rectSB.bottom - rectSB.top;
			if(bResizeBars)
				::SetWindowPos(m_hWndStatusBar, NULL, rect.left, rect.bottom,
					rect.right - rect.left, rectSB.bottom - rectSB.top,
					SWP_NOZORDER | SWP_NOACTIVATE);
		}
	}

	BOOL CreateSimpleStatusBar(LPCTSTR lpstrText, DWORD dwStyle = WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS , UINT nID = ATL_IDW_STATUS_BAR)
	{
		ATLASSERT(!::IsWindow(m_hWndStatusBar));
		m_hWndStatusBar = ::CreateStatusWindow(dwStyle | CCS_NOPARENTALIGN , lpstrText, m_hWnd, nID);
		return (m_hWndStatusBar != NULL);
	}

	BOOL CreateSimpleStatusBar(UINT nTextID = ATL_IDS_IDLEMESSAGE, DWORD dwStyle = WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS , UINT nID = ATL_IDW_STATUS_BAR)
	{
		const int cchMax = 128;   // max text length is 127 for status bars (+1 for null)
		TCHAR szText[cchMax];
		szText[0] = 0;
		::LoadString(ModuleHelper::GetResourceInstance(), nTextID, szText, cchMax);
		return CreateSimpleStatusBar(szText, dwStyle, nID);
	}

	static HWND CreateSimpleReBarCtrl(HWND hWndParent, DWORD dwStyle = ATL_SIMPLE_REBAR_STYLE, UINT nID = ATL_IDW_TOOLBAR)
	{
		return _baseClass::CreateSimpleReBarCtrl(hWndParent, dwStyle | CCS_NOPARENTALIGN, nID);
	}

	BOOL CreateSimpleReBar(DWORD dwStyle = ATL_SIMPLE_REBAR_STYLE, UINT nID = ATL_IDW_TOOLBAR)
	{
		ATLASSERT(!::IsWindow(m_hWndToolBar));
		m_hWndToolBar = _baseClass::CreateSimpleReBarCtrl(m_hWnd, dwStyle | CCS_NOPARENTALIGN, nID);
		return (m_hWndToolBar != NULL);
	}

	BEGIN_MSG_MAP(CAeroFrameImpl)
		CHAIN_MSG_MAP(CAeroImpl<T>)
		CHAIN_MSG_MAP(_baseClass)
	END_MSG_MAP()
};

///////////////////////////////////////////////////////////////////////////////
// CAeroCtrlImpl - implementation of Aero opacity for controls

template <class TBase>
class CAeroCtrlImpl : public CBufferedPaintWindowImpl<CAeroCtrlImpl<TBase>, TBase>
{
public:
	DECLARE_WND_SUPERCLASS(NULL, TBase::GetWndClassName())

	void DoBufferedPaint(CDCHandle dc, RECT& rect)
	{
		HDC hDCPaint = NULL;
		if(IsBufferedPaintSupported())
			m_BufferedPaint.Begin(dc, &rect, m_dwFormat, &m_PaintParams, &hDCPaint);

		if(hDCPaint != NULL)
			DoPaint(hDCPaint, rect);
		else
			DoPaint(dc.m_hDC, rect);

		if(IsBufferedPaintSupported() && !m_BufferedPaint.IsNull())
		{
			m_BufferedPaint.MakeOpaque(&rect);
			m_BufferedPaint.End();
		}
	}

	void DoPaint(CDCHandle dc, RECT& /*rect*/)
	{
		DefWindowProc(WM_PRINTCLIENT, (WPARAM)dc.m_hDC, PRF_CLIENT);
	}
};

///////////////////////////////////////////////////////////////////////////////
// CAeroStatic - Aero opaque Static control
// CAeroButton - Aero opaque Button control

typedef CAeroCtrlImpl<CStatic> CAeroStatic;
typedef CAeroCtrlImpl<CButton> CAeroButton;

///////////////////////////////////////////////////////////////////////////////
// CAeroEdit - Aero opaque Edit control
class CAeroEdit : public CAeroCtrlImpl<CEdit>
{
	BEGIN_MSG_MAP(CAeroEdit)
        REFLECTED_COMMAND_CODE_HANDLER(EN_CHANGE, OnChange)
		CHAIN_MSG_MAP(CAeroCtrlImpl<CEdit>)
	END_MSG_MAP()

	LRESULT OnChange(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& bHandled)
	{
        Invalidate(FALSE); 
		return bHandled = FALSE;
    } 
};

}; // namespace WTL

#endif // __AERO_H__
