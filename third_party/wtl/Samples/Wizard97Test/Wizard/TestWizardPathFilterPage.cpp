
#include "stdafx.h"
#include "TestWizardPathFilterPage.h"

#include "FolderDialogStatusText.h"

LRESULT CTestWizardPathFilterPage::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	CWaitCursor waitCursor;

	this->InitializeControls();
	this->InitializeValues();

	return 1;
}

LRESULT CTestWizardPathFilterPage::OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	// Be sure the base gets the message too
	bHandled = FALSE;

	this->UninitializeControls();	
	return 0;
}

LRESULT CTestWizardPathFilterPage::OnClickFilterAll(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	m_editFilterCustom.SetReadOnly(TRUE);
	return 0;
}

LRESULT CTestWizardPathFilterPage::OnClickFilterCustom(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	m_editFilterCustom.SetReadOnly(FALSE);
	return 0;
}

LRESULT CTestWizardPathFilterPage::OnClickBrowsePath(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	CFolderDialogStatusText dialog(
		m_hWnd, _T("Root directory:"),
		(/*BIF_EDITBOX |*/ BIF_NEWDIALOGSTYLE | BIF_RETURNFSANCESTORS | BIF_RETURNONLYFSDIRS | BIF_STATUSTEXT));

	CString path;
	int cchPath = m_editPath.GetWindowTextLength();
	m_editPath.GetWindowText(path.GetBuffer(cchPath + 1), cchPath + 1);
	path.ReleaseBuffer(cchPath);

	if(path.GetLength() > 0)
	{
		dialog.SetInitialFolder(path);
	}

	if(IDOK == dialog.DoModal())
	{
		m_editPath.SetWindowText(dialog.GetFolderPath());
	}

	return 0;
}

void CTestWizardPathFilterPage::InitializeControls(void)
{
	m_labelPath = this->GetDlgItem(IDC_LABEL_PATH);
	m_editPath = this->GetDlgItem(IDC_EDIT_PATH);
	m_buttonBrowsePath = this->GetDlgItem(IDC_BTN_BROWSEPATH);

	m_radioRecurse = this->GetDlgItem(IDC_RADIO_RECURSE);
	m_radioNoRecurse = this->GetDlgItem(IDC_RADIO_NORECURSE);

	m_labelFilter = this->GetDlgItem(IDC_LABEL_FILTER);
	m_radioFilterAll = this->GetDlgItem(IDC_RADIO_FILTER_ALL);
	m_radioFilterCustom = this->GetDlgItem(IDC_RADIO_FILTER_CUSTOM);
	m_editFilterCustom = this->GetDlgItem(IDC_EDIT_FILTER);

	m_editFilterCustom.SetReadOnly(TRUE);
}

void CTestWizardPathFilterPage::UninitializeControls(void)
{

}

void CTestWizardPathFilterPage::InitializeValues(void)
{
	CString path = m_pTestWizardInfo->GetPath();
	CString filter = m_pTestWizardInfo->GetFilter();
	bool recurse = m_pTestWizardInfo->GetRecurse();

	m_editPath.SetWindowText(path);

	if(recurse)
	{
		m_radioRecurse.Click();
	}
	else
	{
		m_radioNoRecurse.Click();
	}

	if(filter == s_allFiles)
	{
		m_radioFilterAll.Click();
		m_editFilterCustom.SetWindowText(s_allFiles);
	}
	else
	{
		m_radioFilterCustom.Click();
		m_editFilterCustom.SetWindowText(filter);
	}
}

bool CTestWizardPathFilterPage::StoreValues(void)
{
	CString path;
	int cchPath = m_editPath.GetWindowTextLength();
	m_editPath.GetWindowText(path.GetBuffer(cchPath + 1), cchPath + 1);
	path.ReleaseBuffer(cchPath);
	path.TrimLeft();
	path.TrimRight();

	if(path.GetLength() < 1)
	{
		this->MessageBox(
			_T("Please provide a location to find files."),
			_T("No path"), MB_OK | MB_ICONWARNING);
		m_editPath.SetFocus();
		return false;
	}
	else
	{
		m_pTestWizardInfo->SetPath(path);
	}

	bool recurse = (m_radioRecurse.GetCheck() == BST_CHECKED);
	m_pTestWizardInfo->SetRecurse(recurse);

	if(m_radioFilterAll.GetCheck() == BST_CHECKED)
	{
		m_pTestWizardInfo->SetFilter(s_allFiles);
	}
	else
	{
		CString filter;
		int cchFilter = m_editFilterCustom.GetWindowTextLength();
		m_editFilterCustom.GetWindowText(filter.GetBuffer(cchFilter + 1), cchFilter + 1);
		filter.ReleaseBuffer(cchFilter);
		filter.TrimLeft();
		filter.TrimRight();

		if(filter.GetLength() < 1)
		{
			m_radioFilterAll.Click();
			m_editFilterCustom.SetWindowText(s_allFiles);

			m_pTestWizardInfo->SetFilter(s_allFiles);
		}
		else
		{
			m_pTestWizardInfo->SetFilter(filter);
		}
	}

	return true;
}

// Overrides from base class
int CTestWizardPathFilterPage::OnSetActive()
{
	this->SetWizardButtons(PSWIZB_BACK | PSWIZB_NEXT);

	// 0 = allow activate
	// -1 = go back to page that was active
	// page ID = jump to page
	return 0;
}

int CTestWizardPathFilterPage::OnWizardNext()
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

int CTestWizardPathFilterPage::OnWizardBack()
{
	return m_pTestWizardInfo->FindPreviousPage(IDD);
}

void CTestWizardPathFilterPage::OnHelp()
{
	// NOTE: Several controls on this dialog have been given
	//  context sensitive help descriptions, and the HtmlHelp
	//  file is setup to recognize their help IDs.  Please
	//  look at resource.hm, help\Context.h, help\Context.txt
	//  and the help project Wizard97Test.hhp.
	//
	// It's also important to note that context help doesn't
	//  come through this route, but rather goes to the page and
	//  then the sheet (if not handled) as WM_HELP with dwContextId
	//  in the HELPINFO structure set to a non-zero value.
	//  See the sheet for how it deals with context help.
	//  We get to this point if the user clicks on the help button
	//  at the bottom.

	m_pTestWizardInfo->ShowHelp(IDD);
}
