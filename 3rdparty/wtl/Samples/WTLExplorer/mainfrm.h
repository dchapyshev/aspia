// MainFrm.h : interface of the CMainFrame class
//
/////////////////////////////////////////////////////////////////////////////

#ifndef __MAINFRM_H__
#define __MAINFRM_H__

#pragma once

#include <atlframe.h>
#include <atlsplit.h>
#include <atlmisc.h>
#include <atlctrls.h>
#include <atlctrlw.h>
#include <atlctrlx.h>

#include "resource.h"

#include "explorercombo.h"
#include "shellmgr.h"


class CMyPaneContainer : public CPaneContainerImpl<CMyPaneContainer>
{
public:
	DECLARE_WND_CLASS_EX(_T("WtlExplorer_PaneContainer"), 0, -1)

	void DrawPaneTitle(CDCHandle dc)
	{
		RECT rect = { 0 };
		GetClientRect(&rect);

		if(IsVertical())
		{
			rect.right = rect.left + m_cxyHeader;
			dc.DrawEdge(&rect, EDGE_ETCHED, BF_LEFT | BF_TOP | BF_BOTTOM | BF_ADJUST);
			dc.FillRect(&rect, COLOR_3DFACE);
		}
		else
		{
			rect.bottom = rect.top + m_cxyHeader;
// we don't want this edge
//			dc.DrawEdge(&rect, EDGE_ETCHED, BF_LEFT | BF_TOP | BF_RIGHT | BF_ADJUST);
			dc.FillRect(&rect, COLOR_3DFACE);
			// draw title only for horizontal pane container
			dc.SetTextColor(::GetSysColor(COLOR_WINDOWTEXT));
			dc.SetBkMode(TRANSPARENT);
			HFONT hFontOld = dc.SelectFont(GetTitleFont());
			rect.left += m_cxyTextOffset;
			rect.right -= m_cxyTextOffset;
			if(m_tb.m_hWnd != NULL)
				rect.right -= m_cxToolBar;;
			dc.DrawText(m_szTitle, -1, &rect, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
			dc.SelectFont(hFontOld);
		}
	}
};


class CMainFrame : public CFrameWindowImpl<CMainFrame>, 
			public CUpdateUI<CMainFrame>,
			public CMessageFilter, 
			public CIdleHandler
{
private:
	struct SortData
	{
		SortData(int nSortNum, bool bReverse) : nSort(nSortNum), bReverseSort(bReverse)
		{ }

		int nSort;
		bool bReverseSort;
	};

	CCommandBarCtrl m_wndCmdBar;
	CSplitterWindow m_wndSplitter;
///	CPaneContainer m_wndFolderTree;
	CMyPaneContainer m_wndFolderTree;
	CTreeViewCtrlEx m_wndTreeView;
	CListViewCtrl m_wndListView;
	CExplorerCombo m_wndCombo;

	CShellMgr m_ShellMgr;

	int m_nSort;
	bool m_bReverseSort;

	bool m_bFirstIdle;

	// Buffer for OnLVGetDispInfo
	TCHAR m_szListViewBuffer[MAX_PATH];
public:
	DECLARE_FRAME_WND_CLASS(NULL, IDR_MAINFRAME)

	CMainFrame() : m_nSort(0), m_bReverseSort(false), m_bFirstIdle(true)
	{ }

	virtual BOOL PreTranslateMessage(MSG* pMsg)
	{
		return CFrameWindowImpl<CMainFrame>::PreTranslateMessage(pMsg);
	}

	virtual BOOL OnIdle()
	{
		if(m_bFirstIdle)
		{
			CComPtr<IShellFolder> spFolder;
			HRESULT hr = ::SHGetDesktopFolder(&spFolder);
			if(SUCCEEDED(hr))
			{
				CWaitCursor wait;

				m_bFirstIdle = false;

				FillTreeView(spFolder, NULL, TVI_ROOT);
				m_wndTreeView.Expand(m_wndTreeView.GetRootItem());
				m_wndTreeView.SelectItem(m_wndTreeView.GetRootItem());
			}
		}

		UIUpdateToolBar();

		return FALSE;
	}

	HWND CreateAddressBarCtrl(HWND hWndParent);

	void InitViews();

	HRESULT FillTreeView(LPSHELLFOLDER lpsf, LPITEMIDLIST lpifq, HTREEITEM hParent);
	static int CALLBACK CMainFrame::TreeViewCompareProc(LPARAM lparam1, LPARAM lparam2, LPARAM lparamSort);

	BOOL FillListView(LPTVITEMDATA lptvid, LPSHELLFOLDER pShellFolder);
	static int CALLBACK ListViewCompareProc(LPARAM lparam1, LPARAM lparam2, LPARAM lparamSort);

	BEGIN_MSG_MAP(CMainFrame)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		COMMAND_RANGE_HANDLER(ID_VIEW_ICONS, ID_VIEW_LIST, OnViewChange)
		COMMAND_RANGE_HANDLER(ID_VIEW_SORT_NAME, ID_VIEW_SORT_ATTR, OnViewSort)
		COMMAND_ID_HANDLER(ID_COMBO_GO, OnComboGo)
		COMMAND_ID_HANDLER(ID_VIEW_REFRESH, OnViewRefresh)

		NOTIFY_CODE_HANDLER(NM_RCLICK, OnNMRClick)

		NOTIFY_CODE_HANDLER(TVN_SELCHANGED, OnTVSelChanged)
		NOTIFY_CODE_HANDLER(TVN_ITEMEXPANDING, OnTVItemExpanding)
		NOTIFY_CODE_HANDLER(TVN_DELETEITEM, OnTVDeleteItem)

		NOTIFY_CODE_HANDLER(LVN_GETDISPINFO, OnLVGetDispInfo)
		NOTIFY_CODE_HANDLER(LVN_COLUMNCLICK, OnLVColumnClick)
		NOTIFY_CODE_HANDLER(LVN_DELETEITEM, OnLVDeleteItem)
		NOTIFY_CODE_HANDLER(NM_CLICK, OnLVItemClick)
		NOTIFY_CODE_HANDLER(NM_DBLCLK, OnLVItemClick)

		COMMAND_ID_HANDLER(ID_APP_EXIT, OnFileExit)
		COMMAND_ID_HANDLER(ID_FILE_NEW, OnFileNew)
		COMMAND_ID_HANDLER(ID_FILE_NEW_WINDOW, OnFileNewWindow)
		COMMAND_ID_HANDLER(ID_VIEW_TOOLBAR, OnViewToolBar)
		COMMAND_ID_HANDLER(ID_VIEW_ADDRESS_BAR, OnViewAddressBar)
		COMMAND_ID_HANDLER(ID_VIEW_STATUS_BAR, OnViewStatusBar)
		COMMAND_ID_HANDLER(ID_APP_ABOUT, OnAppAbout)
		CHAIN_MSG_MAP(CUpdateUI<CMainFrame>)
		CHAIN_MSG_MAP(CFrameWindowImpl<CMainFrame>)
	END_MSG_MAP()

	BEGIN_UPDATE_UI_MAP(CMainFrame)
		UPDATE_ELEMENT(ID_VIEW_TOOLBAR, UPDUI_MENUPOPUP)
		UPDATE_ELEMENT(ID_VIEW_ADDRESS_BAR, UPDUI_MENUPOPUP)
		UPDATE_ELEMENT(ID_VIEW_STATUS_BAR, UPDUI_MENUPOPUP)

		UPDATE_ELEMENT(ID_VIEW_ICONS, UPDUI_MENUPOPUP)
		UPDATE_ELEMENT(ID_VIEW_SMALL_ICONS, UPDUI_MENUPOPUP)
		UPDATE_ELEMENT(ID_VIEW_LIST, UPDUI_MENUPOPUP)
		UPDATE_ELEMENT(ID_VIEW_DETAILS, UPDUI_MENUPOPUP)

		UPDATE_ELEMENT(ID_VIEW_SORT_NAME, UPDUI_MENUPOPUP)
		UPDATE_ELEMENT(ID_VIEW_SORT_SIZE, UPDUI_MENUPOPUP)
		UPDATE_ELEMENT(ID_VIEW_SORT_TYPE, UPDUI_MENUPOPUP)
		UPDATE_ELEMENT(ID_VIEW_SORT_TIME, UPDUI_MENUPOPUP)
		UPDATE_ELEMENT(ID_VIEW_SORT_ATTR, UPDUI_MENUPOPUP)
	END_UPDATE_UI_MAP()

	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnViewChange(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnComboGo(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnViewRefresh(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnViewSort(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnNMRClick(int , LPNMHDR pnmh, BOOL& );
	LRESULT OnTVSelChanged(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/);
	LRESULT OnTVItemExpanding(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/);
	LRESULT OnTVDeleteItem(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/);
	LRESULT OnLVGetDispInfo(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/);
	LRESULT OnLVColumnClick(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/);
	LRESULT OnLVDeleteItem(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/);
	LRESULT OnLVItemClick(int , LPNMHDR pnmh, BOOL& );
	LRESULT OnFileExit(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnFileNew(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnFileNewWindow(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnViewToolBar(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnViewAddressBar(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnViewStatusBar(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnAppAbout(WORD, WORD, HWND , BOOL& );
};

#endif //__MAINFRM_H__
