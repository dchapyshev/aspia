// ImageViewView.h : interface of the CImageViewView class
//
/////////////////////////////////////////////////////////////////////////////

#if !defined(_IMAGEVIEW_VIEW_H__)
#define _IMAGEVIEW_VIEW_H__

#pragma once

/////////////////////
// CImageViewView :  Application view

class CImageViewView : 
	public CWindowImpl< CImageViewView > ,
	public CZoomScrollImpl< CImageViewView >	
{
public:
	DECLARE_WND_CLASS(NULL)

	CTrackBarCtrl m_ZoomCtrl;
	CStatusBarCtrl m_TitleBar;

	CString m_sImageName;
	CBitmap m_bmp;

	CPoint m_ptMouse;
	
	bool m_bShowScroll;

	CImageViewView() : m_bShowScroll( true) {}

	BOOL PreTranslateMessage(MSG* pMsg)
	{
		pMsg;
		return FALSE;
	}

// Image operation
	void SetImage( HBITMAP hBitmap, LPCTSTR sname = NULL, double fZoom = 1.0, POINT pScroll = CPoint( -1,-1))
	{
		CSize sizeImage( 1, 1);
		m_bmp.Attach( hBitmap ); // will delete existing if necessary

		if( m_bmp.IsNull())
		{
			m_sImageName.Empty();
			m_ZoomCtrl.ModifyStyle( WS_BORDER, NULL, SWP_DRAWFRAME);
			m_TitleBar.SetText( 0, L"No image");
		}
		else
		{
			m_sImageName = sname;
			m_bmp.GetSize( sizeImage);
			m_ZoomCtrl.ModifyStyle( NULL, WS_BORDER, SWP_DRAWFRAME);
			m_TitleBar.SetText( 0, NULL, SBT_OWNERDRAW);
		}

		// Set view zoom factor and center image if no pScroll
		SetZoomScrollSize( sizeImage, fZoom, FALSE );
		SetScrollPage( m_sizeClient);
		if ( CPoint( -1, -1) == pScroll)
			pScroll = (CPoint)(( sizeImage - m_sizeClient) / 2);

		// Hide scroll bars after CZoomScrollImpl settings are complete
		SetScrollOffset( pScroll, m_bShowScroll);
		if ( !m_bShowScroll)
			ShowScrollBars( false);

		// Set trackbar range and position
		sizeImage *= 100;
		CRect rect;
		SystemParametersInfo( SPI_GETWORKAREA, NULL, rect, FALSE);
		m_ZoomCtrl.SetRange( 100, max( sizeImage.cx / rect.Size().cx , sizeImage.cy / rect.Size().cy));
		m_ZoomCtrl.SetPageSize(100);
		m_ZoomCtrl.SetPos( (int)(100 * fZoom ));
	}

// CScrollImpl mandatory override
	void DoPaint( CDCHandle dc)
	{
		if( !m_bmp.IsNull())
			Draw( m_bmp, dc);
	}

// Show/hide scrollbars operation
	void ShowScrollBars( bool bShow)
	{
		m_bShowScroll = bShow;

		if (bShow)
		{
			SCROLLINFO si = { sizeof(si), SIF_PAGE | SIF_RANGE | SIF_POS};

			si.nMax = m_sizeAll.cx - 1;
			si.nPage = m_sizeClient.cx;
			si.nPos = m_ptOffset.x;
			SetScrollInfo(SB_HORZ, &si);

			si.nMax = m_sizeAll.cy - 1;
			si.nPage = m_sizeClient.cy;
			si.nPos = m_ptOffset.y;
			SetScrollInfo(SB_VERT, &si);
		}
		else
		{
			SCROLLINFO si = { sizeof(si), SIF_RANGE, 0, 0};

			SetScrollInfo(SB_HORZ, &si);
			SetScrollInfo(SB_VERT, &si);
		}
		Invalidate();
	}

// CZoomScrollImpl::SetScrollOffset  override for hidden scrollbars
	void SetScrollOffset( POINT ptOffset, BOOL bRedraw = TRUE )
	{
		if ( m_bShowScroll)
			CZoomScrollImpl<CImageViewView>::SetScrollOffset( ptOffset, bRedraw);
		else
		{
			m_ptOffset = CPoint( CSize( ptOffset) / m_fzoom);
			AdjustScrollOffset( (int&)m_ptOffset.x, (int&)m_ptOffset.y);
			if ( bRedraw)
				Invalidate();
		}
	}

// CZoomScrollImpl::SetZoom  override for hidden scrollbars
	void SetZoom( double fzoom, BOOL bRedraw = TRUE )
	{
		if ( m_bShowScroll)
			CZoomScrollImpl<CImageViewView>::SetZoom( fzoom, bRedraw);
		else
		{
			CPoint ptCenter = WndtoTrue( m_sizeClient / 2 );
			m_sizeAll = m_sizeTrue / fzoom;
			m_fzoom = fzoom;
			m_ptOffset = TruetoWnd(ptCenter) + m_ptOffset - m_sizeClient/ 2;
			AdjustScrollOffset( (int&)m_ptOffset.x, (int&)m_ptOffset.y);
			if ( bRedraw)
				Invalidate();
		}
	}
	
// Message map and handlers
	BEGIN_MSG_MAP(CImageViewView)
		MESSAGE_HANDLER(WM_SIZE, OnSize)
		MESSAGE_HANDLER(WM_LBUTTONDOWN, OnLButtonDown)
		MESSAGE_HANDLER(WM_MOUSEMOVE, OnMouseMove)
		MESSAGE_RANGE_HANDLER( WM_KEYFIRST, WM_KEYLAST, OnKey)
		CHAIN_MSG_MAP(CZoomScrollImpl<CImageViewView>)
	ALT_MSG_MAP( 1 )	//	Forwarded by frame
		MESSAGE_HANDLER(WM_CTLCOLORSTATIC, OnZoomColor)
		MESSAGE_HANDLER(WM_HSCROLL, OnZoom)
		MESSAGE_HANDLER(WM_DRAWITEM, OnDrawTitle)
		COMMAND_ID_HANDLER(ID_VIEW_SCROLLBARS, OnShowScrollBars)
		COMMAND_ID_HANDLER(ID_EDIT_COPY, OnCopy)
	END_MSG_MAP()

// CZoomScrollImpl::OnSize  override for hidden scrollbars
	LRESULT OnSize(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& bHandled)
	{
		if ( m_bShowScroll)
			bHandled = FALSE;
		else
		{
			m_sizeClient = CSize( GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
			CPoint ptOffset0 = m_ptOffset;
			if ( AdjustScrollOffset( (int&)m_ptOffset.x, (int&)m_ptOffset.y))
				ScrollWindowEx( ptOffset0.x - m_ptOffset.x, ptOffset0.y - m_ptOffset.y, m_uScrollFlags);
		}
		return bHandled;
	}

// Stylus interaction: tap-and-scroll and context menu
	LRESULT OnLButtonDown(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/)
	{
		m_ptMouse = CPoint( GET_X_LPARAM( lParam), GET_Y_LPARAM( lParam));
		SHRGINFO shrgi = { sizeof(SHRGINFO), m_hWnd, m_ptMouse.x, m_ptMouse.y, SHRG_NOTIFYPARENT};
		return SHRecognizeGesture( &shrgi);
	}

	LRESULT OnMouseMove(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
	{
		if ( 
#ifdef _X86_
			 (wParam & MK_LBUTTON) && 
#endif // _X86_
			 !m_bmp.IsNull())
		{
			CPoint ptNew( GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
			SetScrollOffset( GetScrollOffset() + (( m_ptMouse - ptNew) * GetZoom()));
			m_ptMouse = ptNew;
		}
		return 0;
	}

// Key translation and forwarding
	LRESULT OnKey(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
	{
		switch ( wParam )
		{
			case VK_UP : 
				wParam = VK_PRIOR;
				break;
			case VK_DOWN :
				wParam = VK_NEXT;
				break;
		}
		return m_ZoomCtrl.SendMessage( uMsg, wParam, lParam);
	}

////////////////////////////////////////////////////////////////
//	ALT_MSG_MAP( 1 )  Handlers for forwarded messages

//  Title bar drawing
	LRESULT OnDrawTitle(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/)
	{
		CDCHandle dc = ((LPDRAWITEMSTRUCT)lParam)->hDC;
		CRect rectTitle = ((LPDRAWITEMSTRUCT)lParam)->rcItem;

		dc.FillRect( rectTitle, AtlGetStockBrush( WHITE_BRUSH));

		rectTitle.DeflateRect( 2, 0);
		dc.SetTextColor( RGB( 0, 0, 156));

		CString sTitle = _T("Image: ") + m_sImageName;
		dc.DrawText( sTitle, -1, rectTitle, DT_LEFT | DT_SINGLELINE);

		sTitle.Format( _T("Zoom: %.2f"), GetZoom());
		dc.DrawText( sTitle, -1, rectTitle, DT_RIGHT | DT_SINGLELINE);

		return TRUE;
	}

//  Zoom trackbar interaction
	LRESULT OnZoomColor(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	{
		return (LRESULT)::GetSysColorBrush( m_bmp.IsNull() ? COLOR_BTNFACE : COLOR_BTNHIGHLIGHT );
	}

	LRESULT OnZoom(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	{
		ATLASSERT( !m_bmp.IsNull());
		double fzoom;

		switch LOWORD(wParam)
		{
		case SB_THUMBTRACK :
		case SB_THUMBPOSITION :
			fzoom = (short int)HIWORD(wParam) / 100.0; 
			break;
		default : 
			fzoom = m_ZoomCtrl.GetPos() / 100.0;
		}

		if ( fzoom != m_fzoom)
		{
			SetZoom( fzoom);
			m_TitleBar.SetText( 0, NULL, SBT_OWNERDRAW);
		}
		return TRUE;
	}

//  Scroll bars show-hide
	LRESULT OnShowScrollBars(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		ShowScrollBars( !m_bShowScroll);
		return TRUE;
	}

//  Clipboard copy 
	LRESULT OnCopy(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		if ( !AtlSetClipboardDib16( m_bmp , m_sizeAll, m_hWnd))
			AtlMessageBox( m_hWnd, L"Could not copy image to clipboard", IDR_MAINFRAME, MB_OK | MB_ICONWARNING);
		return 0;
	}
};

#endif // !defined(_IMAGEVIEW_VIEW_H__)
