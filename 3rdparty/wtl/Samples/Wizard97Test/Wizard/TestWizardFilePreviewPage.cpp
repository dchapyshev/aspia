
#include "stdafx.h"
#include "TestWizardFilePreviewPage.h"

///////////////////////////////////////////////////////////////////////////////
// CFileListViewCtrl - a sortable list view of the resulting files

LRESULT CFileListViewCtrl::OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	LRESULT result = DefWindowProc();
	if(result == -1)
	{
		return -1;
	}

	this->Initialize();
	return result;
}

LRESULT CFileListViewCtrl::OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	this->Uninitialize();

	bHandled = FALSE;
	return 0;
}

LRESULT CFileListViewCtrl::OnContextMenu(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& bHandled)
{
	bHandled = TRUE;

	int indexSelectedNearMenu = -1;

	POINT ptPopup = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
	if(ptPopup.x == -1 && ptPopup.y == -1)
	{
		// They used the context menu key or Shift-F10 to bring up the context menu
		indexSelectedNearMenu = this->GetNextItem(-1, LVNI_SELECTED);
		RECT rect = {0};
		if(indexSelectedNearMenu >= 0)
		{
			// If there is a selected item, popup the menu under the first selected item,
			// if not, pop it up in the top left of the list view
			this->GetItemRect(indexSelectedNearMenu, &rect, LVIR_BOUNDS);
			::MapWindowPoints(m_hWnd, NULL, (LPPOINT)&rect, 2);
			ptPopup.x = rect.left;
			ptPopup.y = rect.bottom;
		}
	}
	else
	{
		POINT ptClient = ptPopup;
		::MapWindowPoints(NULL, m_hWnd, &ptClient, 1);

		LVHITTESTINFO hti = { 0 };
		hti.pt = ptClient;
		indexSelectedNearMenu = this->HitTest(&hti);
	}

	if(indexSelectedNearMenu > 0)
	{
		// TODO: Handle multiple selection
		CString fullPath;
		this->GetItemText(indexSelectedNearMenu, ListColumn_FullPath, fullPath);

		// Build up the menu to show
		//CMenu mnuContext;
		//if(mnuContext.CreatePopupMenu())
		//{
		//}
	}

	return 0;
}

void CFileListViewCtrl::Initialize(void)
{
	this->InitializeListColumns();
}

void CFileListViewCtrl::InitializeListColumns(void)
{
	CRect rcList;
	this->GetClientRect(&rcList);

	int width = rcList.Width();
	int columnWidth = 0;
	int remainingWidth = width;

	// NOTE: We'll take the default sort type (LVCOLSORT_TEXT)
	// for the "Name" and "Folder" columns.
	// LVCOLSORT_TEXT uses lstrcmp, which uses the currently
	// selected user locale for sorting. This matches the sorting
	// in the file list in Windows Explorer. With lstrcmp:
	//    "a" < "A" and "A" < "b" and "b" < "B"
	// By comparison, with _tcscmp the sort order is in "ASCII" order:
	//    "A" < "a" and "Z" < "a"
	// LVCOLSORT_TEXTNOCASE uses lstrcmpi, which sorts:
	//    "a" == "A" and "A" < "b" and "b" == "B"

	columnWidth = ::MulDiv(width, 20, 100);  // 20%
	this->InsertColumn(ListColumn_Name, _T("Name"), LVCFMT_LEFT, columnWidth, ListColumn_Name);
	//baseClass::SetColumnSortType(ListColumn_Name, LVCOLSORT_TEXT);
	remainingWidth -= columnWidth;

	columnWidth = ::MulDiv(width, 50, 100);  // 50%
	this->InsertColumn(ListColumn_Folder, _T("In Folder"), LVCFMT_LEFT, columnWidth, ListColumn_Folder);
	//baseClass::SetColumnSortType(ListColumn_Folder, LVCOLSORT_TEXT);
	remainingWidth -= columnWidth;

	columnWidth = ::MulDiv(width, 15, 100);  // 15%
	this->InsertColumn(ListColumn_LastModified, _T("Date Modified"), LVCFMT_LEFT, columnWidth, ListColumn_LastModified);
	baseClass::SetColumnSortType(ListColumn_LastModified, LVCOLSORT_DATETIME);
	remainingWidth -= columnWidth;

	columnWidth = remainingWidth;
	this->InsertColumn(ListColumn_Size, _T("Size"), LVCFMT_LEFT, columnWidth, ListColumn_Size);
	baseClass::SetColumnSortType(ListColumn_Size, LVCOLSORT_CUSTOM);

	// Hidden columns
	// (We could go to more work to keep these truly hidden,
	//  but for now, we'll allow column size adjusts, Ctrl+NumPad+
	//  and the like to reveal the columns).
	this->InsertColumn(ListColumn_SizeBytes, _T("Size in Bytes"), LVCFMT_LEFT, 0, ListColumn_SizeBytes);
	baseClass::SetColumnSortType(ListColumn_SizeBytes, LVCOLSORT_DECIMAL);

	this->InsertColumn(ListColumn_FullPath, _T("Full Path"), LVCFMT_LEFT, 0, ListColumn_FullPath);
	//baseClass::SetColumnSortType(ListColumn_FullPath, LVCOLSORT_TEXT);
}

void CFileListViewCtrl::Uninitialize(void)
{
}

BOOL CFileListViewCtrl::SubclassWindow(HWND hWnd)
{
	ATLASSERT(m_hWnd == NULL);
	ATLASSERT(::IsWindow(hWnd));
	BOOL returnValue = baseClass::SubclassWindow(hWnd);
	if(returnValue)
	{
		this->Initialize();
	}
	return returnValue;
}

HWND CFileListViewCtrl::UnsubclassWindow(BOOL bForce)
{
	this->Uninitialize();

	return baseClass::UnsubclassWindow(bForce);
}

int CFileListViewCtrl::CompareItemsCustom(LVCompareParam* pItem1, LVCompareParam* pItem2, int iSortCol)
{
	int result = 0;

	// Deal with all of the custom sort columns
	switch(iSortCol)
	{
	case ListColumn_Size:
		{
			// Sort based on ListColumn_SizeBytes

			// NOTE: There's other ways to use a "proxy column" for sorting, this is just one
			//  (mainly, just to give an example of CompareItemsCustom).
			//  Another way would be to have DoSortItems run on the hidden column,
			//  but then SetSortColumn for the visible column.

			CString sizeInBytesLHS, sizeInBytesRHS;
			this->GetItemText(pItem1->iItem, ListColumn_SizeBytes, sizeInBytesLHS);
			this->GetItemText(pItem2->iItem, ListColumn_SizeBytes, sizeInBytesRHS);

			__int64 difference = _ttoi64(sizeInBytesRHS) - _ttoi64(sizeInBytesLHS);
			if(difference < 0)
				result = 1;
			else if(difference > 0)
				result = -1;
			else
				result = 0;
		}
		break;
	}

	return result;
}

void CFileListViewCtrl::OnFileFound(LPCTSTR directory, LPWIN32_FIND_DATA findFileData)
{
	TCHAR fileFullPath[MAX_PATH+1] = {0};
	::PathCombine(fileFullPath, directory, findFileData->cFileName);

	ULARGE_INTEGER fileSize = { findFileData->nFileSizeLow, findFileData->nFileSizeHigh };

	this->AddFile(directory, findFileData->cFileName, fileFullPath, findFileData->ftLastWriteTime, fileSize.QuadPart);
}

int CFileListViewCtrl::AddFile(LPCTSTR fileFullPath)
{
	int index = -1;

	WIN32_FILE_ATTRIBUTE_DATA attributes = {0};
	if(::GetFileAttributesEx(fileFullPath, GetFileExInfoStandard, &attributes))
	{
		TCHAR fileSpec[MAX_PATH+1] = {0};
		TCHAR directory[MAX_PATH+1] = {0};

		::lstrcpyn(fileSpec, fileFullPath, MAX_PATH);
		::lstrcpyn(directory, fileFullPath, MAX_PATH);

		::PathStripPath(fileSpec);
		::PathRemoveFileSpec(directory);

		ULARGE_INTEGER fileSize = { attributes.nFileSizeLow, attributes.nFileSizeHigh };

		index = this->AddFile(directory, fileSpec, fileFullPath, attributes.ftLastWriteTime, fileSize.QuadPart);
	}

	return index;
}

int CFileListViewCtrl::AddFile(LPCTSTR directory, LPCTSTR fileSpec, LPCTSTR fileFullPath, FILETIME lastWriteTimeUTC, ULONGLONG fileSize)
{
	FILETIME localTime = {0};
	SYSTEMTIME st = {0};
	::FileTimeToLocalFileTime(&lastWriteTimeUTC, &localTime);
	::FileTimeToSystemTime(&localTime, &st);

	int hour12 = st.wHour;
	if(st.wHour == 0)
		hour12 = 12;
	else if(st.wHour > 12)
		hour12 -= 12;

	CString lastModified;
	lastModified.Format(_T("%d/%d/%d %d:%.2d %s"),
		st.wMonth, st.wDay, st.wYear,
		hour12, st.wMinute, (st.wHour > 11) ? _T("PM") : _T("AM"));

	TCHAR size[32] = {0};
	::StrFormatByteSize64(fileSize, size, 31);

	CString sizeBytes;
	sizeBytes.Format(_T("%I64u"), fileSize);

	int index = this->GetItemCount();
	this->InsertItem(index, fileSpec);
	this->SetItemText(index, ListColumn_Folder, directory);
	this->SetItemText(index, ListColumn_LastModified, lastModified);
	this->SetItemText(index, ListColumn_Size, size);
	this->SetItemText(index, ListColumn_SizeBytes, sizeBytes);
	this->SetItemText(index, ListColumn_FullPath, fileFullPath);

	return index;
}

void CFileListViewCtrl::AutoResizeColumns(void)
{
	this->SetColumnWidth(ListColumn_Name, LVSCW_AUTOSIZE_USEHEADER);
	this->SetColumnWidth(ListColumn_Folder, LVSCW_AUTOSIZE_USEHEADER);
	this->SetColumnWidth(ListColumn_LastModified, LVSCW_AUTOSIZE_USEHEADER);
	this->SetColumnWidth(ListColumn_Size, LVSCW_AUTOSIZE_USEHEADER);
	this->SetColumnWidth(ListColumn_SizeBytes, 0);
	this->SetColumnWidth(ListColumn_FullPath, 0);
}


///////////////////////////////////////////////////////////////////////////////
// CTestWizardFilePreviewPage - Wizard page to preview the files located by the path/filter

LRESULT CTestWizardFilePreviewPage::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	CWaitCursor waitCursor;

	this->InitializeControls();
	this->InitializeValues();

	return 1;
}

LRESULT CTestWizardFilePreviewPage::OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	// Be sure the base gets the message too
	bHandled = FALSE;

	this->UninitializeControls();	
	return 0;
}

LRESULT CTestWizardFilePreviewPage::OnClickPreview(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	CWaitCursor waitCursor;

	m_listFiles.EnableWindow(TRUE);

	this->UpdateFileList();

	return 0;
}

void CTestWizardFilePreviewPage::InitializeControls(void)
{
	m_listFiles.SubclassWindow(this->GetDlgItem(IDC_LIST_FILES));

	m_buttonPreview = this->GetDlgItem(IDC_BTN_PREVIEW);

	m_listFiles.EnableWindow(FALSE);
}

void CTestWizardFilePreviewPage::UninitializeControls(void)
{
	m_listFiles.UnsubclassWindow();
}

void CTestWizardFilePreviewPage::InitializeValues(void)
{
	m_listFiles.DeleteAllItems();
}

bool CTestWizardFilePreviewPage::StoreValues(void)
{
	return true;
}

void CTestWizardFilePreviewPage::UpdateFileList()
{
	CWaitCursor waitCursor;

	m_listFiles.SetSortColumn(-1);

	m_listFiles.SetRedraw(FALSE);
	m_listFiles.DeleteAllItems();

	int fileCount = m_pTestWizardInfo->FindFiles(&m_listFiles);

	if(fileCount < 1)
	{
		m_listFiles.InsertItem(0, _T("(No existing files found for the path and filter)"));
	}

	m_listFiles.AutoResizeColumns();

	m_listFiles.SetRedraw(TRUE);
	m_listFiles.Invalidate();
}

// Overrides from base class
int CTestWizardFilePreviewPage::OnSetActive()
{
	m_listFiles.EnableWindow(FALSE);

	this->SetWizardButtons(PSWIZB_BACK | PSWIZB_NEXT);

	// 0 = allow activate
	// -1 = go back to page that was active
	// page ID = jump to page
	return 0;
}

int CTestWizardFilePreviewPage::OnWizardNext()
{
	bool success = this->StoreValues();
	if(!success)
	{
		// Any errors are already reported, and if appropriate,
		// the control that needs attention has been given focus.
		return -1;
	}

	// 0  = goto next page
	// -1 = prevent page change
	// >0 = jump to page by dlg ID

	return m_pTestWizardInfo->FindNextPage(IDD);
}

int CTestWizardFilePreviewPage::OnWizardBack()
{
	return m_pTestWizardInfo->FindPreviousPage(IDD);
}

void CTestWizardFilePreviewPage::OnHelp()
{
	m_pTestWizardInfo->ShowHelp(IDD);
}
