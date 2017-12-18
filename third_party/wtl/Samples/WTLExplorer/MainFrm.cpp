// mainframe.cpp

#include "stdafx.h"

#include <shlobj.h>
#include <shlguid.h>

#include "mainfrm.h"


BOOL CMainFrame::FillListView(LPTVITEMDATA lptvid, LPSHELLFOLDER pShellFolder)
{
	ATLASSERT(pShellFolder != NULL);

	CComPtr<IEnumIDList> spEnumIDList;
	HRESULT hr = pShellFolder->EnumObjects(m_wndListView.GetParent(), SHCONTF_FOLDERS | SHCONTF_NONFOLDERS, &spEnumIDList);
	if (FAILED(hr))
		return FALSE;

	CShellItemIDList lpifqThisItem;
	CShellItemIDList lpi;
	ULONG ulFetched = 0;
	UINT uFlags = 0;
	LVITEM lvi = { 0 };
	lvi.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_PARAM;
	int iCtr = 0;

	while (spEnumIDList->Next(1, &lpi, &ulFetched) == S_OK)
	{
		// Get some memory for the ITEMDATA structure.
		LPLVITEMDATA lplvid = new LVITEMDATA;
		if (lplvid == NULL) 
			return FALSE;

		// Since you are interested in the display attributes as well as other attributes, 
		// you need to set ulAttrs to SFGAO_DISPLAYATTRMASK before calling GetAttributesOf()
		ULONG ulAttrs = SFGAO_DISPLAYATTRMASK;
		hr = pShellFolder->GetAttributesOf(1, (const struct _ITEMIDLIST **)&lpi, &ulAttrs);
		if(FAILED(hr)) 
			return FALSE;

		lplvid->ulAttribs = ulAttrs;

		lpifqThisItem = m_ShellMgr.ConcatPidls(lptvid->lpifq, lpi);

		lvi.iItem = iCtr;
		lvi.iSubItem = 0;
		lvi.pszText = LPSTR_TEXTCALLBACK;
		lvi.cchTextMax = MAX_PATH;
		uFlags = SHGFI_PIDL | SHGFI_SYSICONINDEX | SHGFI_SMALLICON;
		lvi.iImage = I_IMAGECALLBACK;

		lplvid->spParentFolder.p = pShellFolder;
		pShellFolder->AddRef();

		// Now make a copy of the ITEMIDLIST
		lplvid->lpi= m_ShellMgr.CopyITEMID(lpi);

		lvi.lParam = (LPARAM)lplvid;

		// Add the item to the list view control
		int n = m_wndListView.InsertItem(&lvi);
		m_wndListView.AddItem(n, 1, LPSTR_TEXTCALLBACK, I_IMAGECALLBACK);
		m_wndListView.AddItem(n, 2, LPSTR_TEXTCALLBACK, I_IMAGECALLBACK);
		m_wndListView.AddItem(n, 3, LPSTR_TEXTCALLBACK, I_IMAGECALLBACK);
		m_wndListView.AddItem(n, 4, LPSTR_TEXTCALLBACK, I_IMAGECALLBACK);

		iCtr++;
		lpifqThisItem = NULL;
		lpi = NULL;   // free PIDL the shell gave you
	}

	SortData sd(m_nSort, m_bReverseSort);
	m_wndListView.SortItems(CMainFrame::ListViewCompareProc, (LPARAM)&sd);

	return TRUE;
}

int CALLBACK CMainFrame::ListViewCompareProc(LPARAM lparam1, LPARAM lparam2, LPARAM lParamSort)
{
	ATLASSERT(lParamSort != NULL);

	LPLVITEMDATA lplvid1 = (LPLVITEMDATA)lparam1;
	LPLVITEMDATA lplvid2 = (LPLVITEMDATA)lparam2;
	SortData* pSD = (SortData*)lParamSort;

	HRESULT hr = 0;
	if(pSD->bReverseSort)
		hr = lplvid1->spParentFolder->CompareIDs(0, lplvid2->lpi, lplvid1->lpi);
	else
		hr = lplvid1->spParentFolder->CompareIDs(0, lplvid1->lpi, lplvid2->lpi);

	return (int)(short)HRESULT_CODE(hr);
}

HRESULT CMainFrame::FillTreeView(IShellFolder* pShellFolder, LPITEMIDLIST lpifq, HTREEITEM hParent)
{
	if(pShellFolder == NULL)
		return E_POINTER;

	CComPtr<IEnumIDList> spIDList;
	HRESULT hr = pShellFolder->EnumObjects(m_hWnd, SHCONTF_FOLDERS | SHCONTF_NONFOLDERS, &spIDList);
	if (FAILED(hr))
		return hr;

	CShellItemIDList lpi;
	CShellItemIDList lpifqThisItem;
	LPTVITEMDATA lptvid = NULL;
	ULONG ulFetched	= 0;

	TCHAR szBuff[256] = { 0 };

	TVITEM tvi = { 0 };             // TreeView Item
	TVINSERTSTRUCT tvins = { 0 };   // TreeView Insert Struct
	HTREEITEM hPrev = NULL;         // Previous Item Added
	COMBOBOXEXITEM cbei = { 0 };

	// Hourglass on
	CWaitCursor wait;

	int iCnt = 0;
	while (spIDList->Next(1, &lpi, &ulFetched) == S_OK)
	{
		// Create a fully qualified path to the current item
		// The SH* shell api's take a fully qualified path pidl,
		// (see GetIcon above where I call SHGetFileInfo) whereas the
		// interface methods take a relative path pidl.
		ULONG ulAttrs = SFGAO_HASSUBFOLDER | SFGAO_FOLDER;
		pShellFolder->GetAttributesOf(1, (LPCITEMIDLIST*)&lpi, &ulAttrs);
		if ((ulAttrs & (SFGAO_HASSUBFOLDER | SFGAO_FOLDER)) != 0)
		{
			// We need this next if statement so that we don't add things like
			// the MSN to our tree. MSN is not a folder, but according to the
			// shell is has subfolders....
			if (ulAttrs & SFGAO_FOLDER)
			{
				tvi.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM;
				cbei.mask = CBEIF_TEXT | CBEIF_INDENT | CBEIF_IMAGE | CBEIF_SELECTEDIMAGE;

				if (ulAttrs & SFGAO_HASSUBFOLDER)
				{
					// This item has sub-folders, so let's put the + in the TreeView.
					// The first time the user clicks on the item, we'll populate the
					// sub-folders then.
					tvi.cChildren = 1;
					tvi.mask |= TVIF_CHILDREN;
				}

				// OK, let's get some memory for our ITEMDATA struct
				lptvid = new TVITEMDATA;
				if (lptvid == NULL)
					return E_FAIL;

				// Now get the friendly name that we'll put in the treeview...
				if (!m_ShellMgr.GetName(pShellFolder, lpi, SHGDN_NORMAL, szBuff))
					return E_FAIL;

				tvi.pszText = szBuff;
				tvi.cchTextMax = MAX_PATH;

				cbei.pszText = szBuff;
				cbei.cchTextMax = MAX_PATH;

				lpifqThisItem = m_ShellMgr.ConcatPidls(lpifq, lpi);

				// Now, make a copy of the ITEMIDLIST
				lptvid->lpi=m_ShellMgr.CopyITEMID(lpi);

				m_ShellMgr.GetNormalAndSelectedIcons(lpifqThisItem, &tvi);

				lptvid->spParentFolder.p = pShellFolder;    // Store the parent folders SF
				pShellFolder->AddRef();

				lptvid->lpifq = m_ShellMgr.ConcatPidls(lpifq, lpi);

				tvi.lParam = (LPARAM)lptvid;

				tvins.item = tvi;
				tvins.hInsertAfter = hPrev;
				tvins.hParent = hParent;

				// Add the item to the tree and combo
				hPrev = TreeView_InsertItem(m_wndTreeView.m_hWnd, &tvins);
				cbei.iItem = iCnt++;	
				cbei.iImage = tvi.iImage;
				cbei.iSelectedImage = tvi.iSelectedImage;

				int nIndent = 0;
				while(NULL != (hPrev = (HTREEITEM)m_wndTreeView.SendMessage(TVM_GETNEXTITEM, TVGN_PARENT, (LPARAM)hPrev)))
				{
					nIndent++;
				}

				cbei.iIndent = nIndent;
				m_wndCombo.SendMessage(CBEM_INSERTITEM, 0, (LPARAM)&cbei);
			}

			lpifqThisItem = NULL;
		}

		lpi = NULL;   // Finally, free the pidl that the shell gave us...
	}

	return hr;
}

int CALLBACK CMainFrame::TreeViewCompareProc(LPARAM lparam1, LPARAM lparam2, LPARAM /*lparamSort*/)
{
	LPTVITEMDATA lptvid1 = (LPTVITEMDATA)lparam1;
	LPTVITEMDATA lptvid2 = (LPTVITEMDATA)lparam2;

	HRESULT hr = lptvid1->spParentFolder->CompareIDs(0, lptvid1->lpi, lptvid2->lpi);

	return (int)(short)HRESULT_CODE(hr);
}

HWND CMainFrame::CreateAddressBarCtrl(HWND hWndParent)
{
	RECT rc = { 50, 0, 300, 100 };
	m_wndCombo.Create(hWndParent, rc, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | CBS_DROPDOWN | CBS_AUTOHSCROLL);

	return m_wndCombo;
}

LRESULT CMainFrame::OnCreate(UINT, WPARAM, LPARAM, BOOL&)
{
	
	// create command bar window
	RECT rcCmdBar = { 0, 0, 100, 100 };
	HWND hWndCmdBar = m_wndCmdBar.Create(m_hWnd, rcCmdBar, NULL, ATL_SIMPLE_CMDBAR_PANE_STYLE);
	// atach menu
	m_wndCmdBar.AttachMenu(GetMenu());
	// load command bar images
	m_wndCmdBar.LoadImages(IDR_MAINFRAME);
	// remove old menu
	SetMenu(NULL);

	HWND hWndToolBar = CreateSimpleToolBarCtrl(m_hWnd, IDR_MAINFRAME, FALSE, ATL_SIMPLE_TOOLBAR_PANE_STYLE);
	HWND hWndAddressBar = CreateAddressBarCtrl(m_hWnd);

	CreateSimpleReBar(ATL_SIMPLE_REBAR_NOBORDER_STYLE);

	AddSimpleReBarBand(hWndCmdBar);
	AddSimpleReBarBand(hWndToolBar, NULL, TRUE);
	AddSimpleReBarBand(hWndAddressBar, _T("Address"), TRUE);

	CreateSimpleStatusBar();

	m_hWndClient = m_wndSplitter.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);

	m_wndFolderTree.Create(m_wndSplitter, _T("Folders"), WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);
	
	m_wndTreeView.Create(m_wndFolderTree, rcDefault, NULL, 
		WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | 
		TVS_HASLINES | TVS_LINESATROOT | TVS_HASBUTTONS | TVS_SHOWSELALWAYS, 
		WS_EX_CLIENTEDGE);

	m_wndFolderTree.SetClient(m_wndTreeView);

	m_wndListView.Create(m_wndSplitter, rcDefault, NULL, 
		WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | 
		LVS_REPORT | LVS_AUTOARRANGE | LVS_SHOWSELALWAYS | LVS_SHAREIMAGELISTS,
		WS_EX_CLIENTEDGE);
	m_wndListView.SetExtendedListViewStyle(LVS_EX_TRACKSELECT | LVS_EX_ONECLICKACTIVATE | LVS_EX_UNDERLINEHOT);
	
	InitViews();

	UpdateLayout();

	m_wndSplitter.SetSplitterPanes(m_wndFolderTree, m_wndListView);

	RECT rect;
	GetClientRect(&rect);
	m_wndSplitter.SetSplitterPos((rect.right - rect.left) / 4);

	UIAddToolBar(hWndToolBar);
	UISetCheck(ID_VIEW_TOOLBAR, 1);
	UISetCheck(ID_VIEW_ADDRESS_BAR, 1);
	UISetCheck(ID_VIEW_STATUS_BAR, 1);
	UISetCheck(ID_VIEW_DETAILS, 1);
	UISetCheck(ID_VIEW_SORT_NAME, 1);

	CMessageLoop* pLoop = _Module.GetMessageLoop();
	pLoop->AddMessageFilter(this);
	pLoop->AddIdleHandler(this);

	return 0;
}

LRESULT CMainFrame::OnViewChange(WORD, WORD wID, HWND, BOOL&)
{
	UISetCheck(ID_VIEW_ICONS, FALSE);
	UISetCheck(ID_VIEW_SMALL_ICONS, FALSE);
	UISetCheck(ID_VIEW_LIST, FALSE);
	UISetCheck(ID_VIEW_DETAILS, FALSE);
	UISetCheck(wID, TRUE);
	DWORD dwNewStyle = (int)wID - ID_VIEW_ICONS;
	m_wndListView.SetViewType(dwNewStyle);

	return 0;
}

LRESULT CMainFrame::OnComboGo(WORD, WORD, HWND, BOOL&)
{
//	TCHAR szBuff[MAX_PATH] = { 0 };
//	m_wndCombo.GetWindowText(szBuff, MAX_PATH);
//		
//	m_wndTreeView.SelectItem(NULL);
	//m_wndCombo.SetEditSel(0, -1);
	//FillListView(m_wndListView, szBuff);

	MessageBox(_T("Not yet implemented!"), _T("WTL Explorer"), MB_OK | MB_ICONINFORMATION);

	return 0;
}

LRESULT CMainFrame::OnViewRefresh(WORD, WORD, HWND, BOOL&)
{
	MessageBox(_T("Not yet implemented!"), _T("WTL Explorer"), MB_OK | MB_ICONINFORMATION);

	return 0;
}

LRESULT CMainFrame::OnViewSort(WORD, WORD wID, HWND, BOOL&)
{
	UISetCheck(ID_VIEW_SORT_NAME, FALSE);
	UISetCheck(ID_VIEW_SORT_SIZE, FALSE);
	UISetCheck(ID_VIEW_SORT_TYPE, FALSE);
	UISetCheck(ID_VIEW_SORT_TIME, FALSE);
	UISetCheck(ID_VIEW_SORT_ATTR, FALSE);
	UISetCheck(wID, TRUE);
	m_bReverseSort = false;
	m_nSort = (int)wID - ID_VIEW_SORT_NAME;
	ATLASSERT(m_nSort >= 0 && m_nSort <= 4);
	SortData sd(m_nSort, m_bReverseSort);
	m_wndListView.SortItems(CMainFrame::ListViewCompareProc, (LPARAM)&sd);

	return 0;
}

LRESULT CMainFrame::OnTVSelChanged(int, LPNMHDR pnmh, BOOL&)
{
	LPNMTREEVIEW lpTV = (LPNMTREEVIEW)pnmh;
	LPTVITEMDATA lptvid = (LPTVITEMDATA)lpTV->itemNew.lParam;
	if (lptvid != NULL)
	{
		CComPtr<IShellFolder> spFolder;
		HRESULT hr=lptvid->spParentFolder->BindToObject(lptvid->lpi, 0, IID_IShellFolder, (LPVOID *)&spFolder);
		if(FAILED(hr))
			return hr;
		
		if(m_wndListView.GetItemCount() > 0)
			m_wndListView.DeleteAllItems();
		FillListView(lptvid, spFolder);
		
		TCHAR psz[MAX_PATH] = { 0 };
		m_ShellMgr.GetName(lptvid->spParentFolder, lptvid->lpi, SHGDN_FORPARSING, psz);
		
		if(StrChr(psz, _T('{')))   //special case
			m_ShellMgr.GetName(lptvid->spParentFolder, lptvid->lpi, SHGDN_NORMAL, psz);
	
		int nImage = 0;
		int nSelectedImage = 0;
		m_wndTreeView.GetItemImage(lpTV->itemNew.hItem, nImage, nSelectedImage);
		COMBOBOXEXITEM cbei;
		cbei.mask = CBEIF_TEXT | CBEIF_INDENT |CBEIF_IMAGE| CBEIF_SELECTEDIMAGE;
		cbei.iItem = -1; //Editbox of the comboboxex
		cbei.pszText =psz;
		cbei.cchTextMax = MAX_PATH;
		if(m_wndTreeView.ItemHasChildren(lpTV->itemNew.hItem))
		{
			cbei.iImage = nImage;
			cbei.iSelectedImage = nImage;
		}
		else 
		{
			cbei.iImage = nSelectedImage;
			cbei.iSelectedImage = nSelectedImage;
		}
		cbei.iIndent = 1;
		
		m_wndCombo.SetItem(&cbei);
		
	}

	return 0L;
}

LRESULT CMainFrame::OnTVItemExpanding(int, LPNMHDR pnmh, BOOL&)
{
	CWaitCursor wait;
	LPNMTREEVIEW pnmtv = (LPNMTREEVIEW)pnmh;
	if ((pnmtv->itemNew.state & TVIS_EXPANDEDONCE))
		 return 0L;

	LPTVITEMDATA lptvid=(LPTVITEMDATA)pnmtv->itemNew.lParam;
	
	CComPtr<IShellFolder> spFolder;
	HRESULT hr=lptvid->spParentFolder->BindToObject(lptvid->lpi, 0, IID_IShellFolder, (LPVOID *)&spFolder);
	if(FAILED(hr))
		return hr;
	
	FillTreeView(spFolder, lptvid->lpifq, pnmtv->itemNew.hItem);
	
	TVSORTCB tvscb;
	tvscb.hParent = pnmtv->itemNew.hItem;
	tvscb.lpfnCompare = CMainFrame::TreeViewCompareProc;
	tvscb.lParam = 0;

	TreeView_SortChildrenCB(m_wndTreeView.m_hWnd, &tvscb, 0);
	
	return 0L;

}

LRESULT CMainFrame::OnTVDeleteItem(int, LPNMHDR pnmh, BOOL&)
{
	LPNMTREEVIEW pnmtv = (LPNMTREEVIEW)pnmh;
	LPTVITEMDATA lptvid = (LPTVITEMDATA)pnmtv->itemOld.lParam;
	delete lptvid;

	return 0;
}

LRESULT CMainFrame::OnLVGetDispInfo(int, LPNMHDR pnmh, BOOL&)
{
	NMLVDISPINFO* plvdi = (NMLVDISPINFO*)pnmh;
	if(plvdi == NULL)
		return 0L;

	LPLVITEMDATA lplvid = (LPLVITEMDATA)plvdi->item.lParam;

	HTREEITEM hti = m_wndTreeView.GetSelectedItem();
	TVITEM tvi = { 0 };
	tvi.mask = TVIF_PARAM;
	tvi.hItem = hti;

	m_wndTreeView.GetItem(&tvi);
	if(tvi.lParam <= 0)
		return 0L;

	LPTVITEMDATA lptvid = (LPTVITEMDATA)tvi.lParam;
	if (lptvid == NULL)
		return 0L;
	
	CShellItemIDList pidlTemp = m_ShellMgr.ConcatPidls(lptvid->lpifq, lplvid->lpi);
	plvdi->item.iImage = m_ShellMgr.GetIconIndex(pidlTemp, SHGFI_PIDL | SHGFI_SYSICONINDEX | SHGFI_SMALLICON);
	if (plvdi->item.iSubItem == 0 && (plvdi->item.mask & LVIF_TEXT) )   // File Name
	{
		m_ShellMgr.GetName(lplvid->spParentFolder, lplvid->lpi, SHGDN_NORMAL, plvdi->item.pszText);
	}	
	else
	{
		CComPtr<IShellFolder2> spFolder2;
		HRESULT hr = lptvid->spParentFolder->QueryInterface(IID_IShellFolder2, (void**)&spFolder2);
		if(FAILED(hr))
			return hr;

		SHELLDETAILS sd = { 0 };
		sd.fmt = LVCFMT_CENTER;
		sd.cxChar = 15;
		
		hr = spFolder2->GetDetailsOf(lplvid->lpi, plvdi->item.iSubItem, &sd);
		if(FAILED(hr))
			return hr;

		if(sd.str.uType == STRRET_WSTR)
		{
			StrRetToBuf(&sd.str, lplvid->lpi.m_pidl, m_szListViewBuffer, MAX_PATH);
			plvdi->item.pszText=m_szListViewBuffer;
		}
		else if(sd.str.uType == STRRET_OFFSET)
		{
			plvdi->item.pszText = (LPTSTR)lptvid->lpi + sd.str.uOffset;
		}
		else if(sd.str.uType == STRRET_CSTR)
		{
			USES_CONVERSION;
			plvdi->item.pszText = A2T(sd.str.cStr);
		}
	}
	
	plvdi->item.mask |= LVIF_DI_SETITEM;

	return 0L;
}

LRESULT CMainFrame::OnLVColumnClick(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	LPNMLISTVIEW lpLV = (LPNMLISTVIEW)pnmh;
	if(m_nSort == lpLV->iSubItem)
		m_bReverseSort = !m_bReverseSort;
	else
		m_nSort = lpLV->iSubItem;
	ATLASSERT(m_nSort >= 0 && m_nSort <= 4);
	SortData sd(m_nSort, m_bReverseSort);
	m_wndListView.SortItems(CMainFrame::ListViewCompareProc, (LPARAM)&sd);

	return 0;
}

LRESULT CMainFrame::OnNMRClick(int, LPNMHDR pnmh, BOOL&)
{
	POINT pt = { 0, 0 };
	::GetCursorPos(&pt);
	POINT ptClient = pt;
	if(pnmh->hwndFrom != NULL)
		::ScreenToClient(pnmh->hwndFrom, &ptClient);
	
	if(pnmh->hwndFrom == m_wndTreeView.m_hWnd)
	{
		TVHITTESTINFO tvhti = { 0 };
		tvhti.pt = ptClient;
		m_wndTreeView.HitTest(&tvhti);
		if ((tvhti.flags & TVHT_ONITEMLABEL) != 0)
		{
			TVITEM tvi = { 0 };
			tvi.mask = TVIF_PARAM;
			tvi.hItem = tvhti.hItem;
			if (m_wndTreeView.GetItem(&tvi) != FALSE)
			{
				LPTVITEMDATA lptvid = (LPTVITEMDATA)tvi.lParam;
				if (lptvid != NULL)
					m_ShellMgr.DoContextMenu(::GetParent(m_wndTreeView.m_hWnd), lptvid->spParentFolder, lptvid->lpi, pt);
			}
		}
	}
	else if(pnmh->hwndFrom == m_wndListView.m_hWnd)
	{
		LVHITTESTINFO lvhti = { 0 };
		lvhti.pt = ptClient;
		m_wndListView.HitTest(&lvhti);
		if ((lvhti.flags & LVHT_ONITEMLABEL) != 0)
		{
			LVITEM lvi = { 0 };
			lvi.mask = LVIF_PARAM;
			lvi.iItem = lvhti.iItem;
			if (m_wndListView.GetItem(&lvi) != FALSE)
			{
				LPLVITEMDATA lptvid = (LPLVITEMDATA)lvi.lParam;
				if (lptvid != NULL)
					m_ShellMgr.DoContextMenu(::GetParent(m_wndListView.m_hWnd), lptvid->spParentFolder, lptvid->lpi, pt);
			}
		}
	}

	return 0L;
}

LRESULT CMainFrame::OnLVItemClick(int, LPNMHDR pnmh, BOOL&)
{

	if(pnmh->hwndFrom != m_wndListView.m_hWnd)
		return 0L;
	POINT pt;
	::GetCursorPos((LPPOINT)&pt);
	m_wndListView.ScreenToClient(&pt);

	LVHITTESTINFO lvhti;
	lvhti.pt = pt;
	m_wndListView.HitTest(&lvhti);
	LVITEM lvi;

	if (lvhti.flags & LVHT_ONITEM)
	{
		m_wndListView.ClientToScreen(&pt);
		lvi.mask = LVIF_PARAM;
		lvi.iItem = lvhti.iItem;
		lvi.iSubItem = 0;

		if (!m_wndListView.GetItem(&lvi))
			return 0;

		LPLVITEMDATA lplvid=(LPLVITEMDATA)lvi.lParam;

		if(lplvid == NULL)
		{
			return 0L;
		}
		else if ((lplvid->ulAttribs & SFGAO_FOLDER) == 0)
		{
			SHELLEXECUTEINFO sei =
			{
				sizeof(SHELLEXECUTEINFO),
				SEE_MASK_INVOKEIDLIST,               // fMask
				::GetParent(m_wndListView.m_hWnd),   // hwnd of parent
				_T("Open"),                          // lpVerb
				NULL,                                // lpFile
				_T(""),
				_T(""),                              // lpDirectory
				SW_SHOWNORMAL,                       // nShow
				_Module.GetModuleInstance(),         // hInstApp
				(LPVOID)NULL,                        // lpIDList...will set below
				NULL,                                // lpClass
				0,                                   // hkeyClass
				0,                                   // dwHotKey
				NULL                                 // hIcon
			};
			sei.lpIDList = m_ShellMgr.GetFullyQualPidl(lplvid->spParentFolder, lplvid->lpi);
			::ShellExecuteEx(&sei);
		}
		else
		{
			MessageBox(_T("Clicked on folder"));
		}
	}

	return 0L;
}

LRESULT CMainFrame::OnLVDeleteItem(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	LPNMLISTVIEW pnmlv = (LPNMLISTVIEW)pnmh;

	LVITEM lvi;
	lvi.mask = LVIF_PARAM;
	lvi.iItem = pnmlv->iItem;
	lvi.iSubItem = 0;

	if (!m_wndListView.GetItem(&lvi))
		return 0;

	LPLVITEMDATA lplvid = (LPLVITEMDATA)lvi.lParam;
	delete lplvid;

	return 0;
}

LRESULT CMainFrame::OnFileExit(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	PostMessage(WM_CLOSE);
	return 0;
}

LRESULT CMainFrame::OnFileNew(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	// TODO: add code to initialize document

	return 0;
}

LRESULT CMainFrame::OnFileNewWindow(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	::PostThreadMessage(_Module.m_dwMainThreadID, WM_USER, 0, 0L);
	return 0;
}

LRESULT CMainFrame::OnViewToolBar(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	static BOOL bNew = TRUE;	// initially visible
	bNew = !bNew;
	::SendMessage(m_hWndToolBar, RB_SHOWBAND, 1, bNew);	// toolbar is band #1
	UISetCheck(ID_VIEW_TOOLBAR, bNew);
	UpdateLayout();
	return 0;
}

LRESULT CMainFrame::OnViewAddressBar(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	static BOOL bNew = TRUE;	// initially visible
	bNew = !bNew;
	::SendMessage(m_hWndToolBar, RB_SHOWBAND, 2, bNew);	// address bar is band #2
	UISetCheck(ID_VIEW_ADDRESS_BAR, bNew);
	UpdateLayout();
	return 0;
}

LRESULT CMainFrame::OnViewStatusBar(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	BOOL bNew = !::IsWindowVisible(m_hWndStatusBar);
	::ShowWindow(m_hWndStatusBar, bNew ? SW_SHOWNOACTIVATE : SW_HIDE);
	UISetCheck(ID_VIEW_STATUS_BAR, bNew);
	UpdateLayout();
	return 0;
}

LRESULT CMainFrame::OnAppAbout(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	::ShellAbout(m_hWnd, _T("WTL Explorer"), _T("This sample program is a part \nof the WTL Library"), AtlLoadIcon(IDR_MAINFRAME));
	return 0;
}

void CMainFrame::InitViews()
{
	// Get Desktop folder
	CShellItemIDList spidl;
	HRESULT hRet = ::SHGetSpecialFolderLocation(m_hWnd, CSIDL_DESKTOP, &spidl);
	hRet;	// avoid level 4 warning
	ATLASSERT(SUCCEEDED(hRet));

	// Get system image lists
	SHFILEINFO sfi = { 0 };
	HIMAGELIST hImageList = (HIMAGELIST)::SHGetFileInfo(spidl, 0, &sfi, sizeof(sfi), SHGFI_PIDL | SHGFI_SYSICONINDEX | SHGFI_ICON);
	ATLASSERT(hImageList != NULL);

	memset(&sfi, 0, sizeof(SHFILEINFO));
	HIMAGELIST hImageListSmall = (HIMAGELIST)::SHGetFileInfo(spidl, 0, &sfi, sizeof(sfi), SHGFI_PIDL | SHGFI_SYSICONINDEX | SHGFI_SMALLICON);
	ATLASSERT(hImageListSmall != NULL);

	// Set address bar combo box image list
	m_wndCombo.SetImageList(hImageListSmall);

	// Set tree view image list
	m_wndTreeView.SetImageList(hImageListSmall, 0);

	// Create list view columns
	m_wndListView.InsertColumn(0, _T("Name"), LVCFMT_LEFT, 200, 0);
	m_wndListView.InsertColumn(1, _T("Size"), LVCFMT_RIGHT, 100, 1);
	m_wndListView.InsertColumn(2, _T("Type"), LVCFMT_LEFT, 100, 2);
	m_wndListView.InsertColumn(3, _T("Modified"), LVCFMT_LEFT, 100, 3);
	m_wndListView.InsertColumn(4, _T("Attributes"), LVCFMT_RIGHT, 100, 4);

	// Set list view image lists
	m_wndListView.SetImageList(hImageList, LVSIL_NORMAL);
	m_wndListView.SetImageList(hImageListSmall, LVSIL_SMALL);
}
