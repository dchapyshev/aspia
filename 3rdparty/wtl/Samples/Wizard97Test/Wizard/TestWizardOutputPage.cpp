
#include "stdafx.h"
#include "TestWizardOutputPage.h"

LRESULT CTestWizardOutputPage::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	CWaitCursor waitCursor;

	this->InitializeControls();
	this->InitializeValues();

	return 1;
}

LRESULT CTestWizardOutputPage::OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	// Be sure the base gets the message too
	bHandled = FALSE;

	this->UninitializeControls();	
	return 0;
}

LRESULT CTestWizardOutputPage::OnClickCopyToClipboard(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	m_labelSaveFileName.EnableWindow(FALSE);
	m_editFileName.EnableWindow(FALSE);
	m_buttonBrowseFile.EnableWindow(FALSE);
	m_labelFileEncoding.EnableWindow(FALSE);
	m_comboFileEncoding.EnableWindow(FALSE);
	return 0;
}

LRESULT CTestWizardOutputPage::OnClickSendEmail(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	m_labelSaveFileName.EnableWindow(FALSE);
	m_editFileName.EnableWindow(FALSE);
	m_buttonBrowseFile.EnableWindow(FALSE);
	m_labelFileEncoding.EnableWindow(FALSE);
	m_comboFileEncoding.EnableWindow(FALSE);
	return 0;
}

LRESULT CTestWizardOutputPage::OnClickSaveToFile(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	m_labelSaveFileName.EnableWindow(TRUE);
	m_editFileName.EnableWindow(TRUE);
	m_buttonBrowseFile.EnableWindow(TRUE);
	m_labelFileEncoding.EnableWindow(TRUE);
	m_comboFileEncoding.EnableWindow(TRUE);
	return 0;
}

LRESULT CTestWizardOutputPage::OnClickBrowseFileName(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	CFileDialog dialog(FALSE,
		_T("txt"), NULL,
		OFN_EXPLORER | OFN_HIDEREADONLY | OFN_ENABLESIZING | OFN_NOCHANGEDIR | OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST,
		_T("Text Documents (*.txt)\0*.txt\0All Files (*.*)\0*.*\0"), m_hWnd);
	dialog.m_ofn.lpstrTitle = _T("Select where to output the file list");

	CString initialDirectory = m_pTestWizardInfo->GetPath();

	CString fileName;
	int cchFileName = m_editFileName.GetWindowTextLength();
	m_editFileName.GetWindowText(fileName.GetBuffer(cchFileName + 1), cchFileName + 1);
	fileName.ReleaseBuffer(cchFileName);
	fileName.TrimLeft();
	fileName.TrimRight();

	if(fileName.GetLength() > 0)
	{
		initialDirectory = fileName;
		::PathRemoveFileSpec(initialDirectory.GetBuffer(0));
		initialDirectory.ReleaseBuffer();
	}

	if((initialDirectory.GetLength() > 0) && (::GetFileAttributes(initialDirectory) != INVALID_FILE_ATTRIBUTES))
	{
		dialog.m_ofn.lpstrInitialDir = initialDirectory;
	}

	INT_PTR dialogResult = dialog.DoModal();
	if(IDOK == dialogResult)
	{
		m_editFileName.SetWindowText(dialog.m_szFileName);
	}

	return 0;
}

void CTestWizardOutputPage::InitializeControls(void)
{
	m_radioCopyToClipboard = this->GetDlgItem(IDC_RADIO_COPYTOCLIPBOARD);
	m_radioSendEmail = this->GetDlgItem(IDC_RADIO_SENDEMAIL);
	m_radioSaveToFile = this->GetDlgItem(IDC_RADIO_SAVETOFILE);

	m_labelSaveFileName = this->GetDlgItem(IDC_LABEL_FILENAME);
	m_editFileName = this->GetDlgItem(IDC_EDIT_SAVETOFILE);
	m_buttonBrowseFile = this->GetDlgItem(IDC_BTN_FILEBROWSE);
	m_labelFileEncoding = this->GetDlgItem(IDC_LABEL_FILEENCODING);
	m_comboFileEncoding = this->GetDlgItem(IDC_COMBO_FILEENCODING);

	for(int i=(int)eEncoding_First; i<=(int)eEncoding_Last; ++i)
	{
		m_comboFileEncoding.AddString(
			CTestWizardInfo::GetOutputFileEncodingDisplayName((TestWizardOutputFileEncoding)i));
	}

	m_comboFileEncoding.SetCurSel(0);

	// It's possible to have more control over the auto-complete functionality.
	// See MSDN for info about IAutoComplete2, IACList2, and so on.
	::SHAutoComplete(m_editFileName, SHACF_FILESYSTEM | SHACF_AUTOAPPEND_FORCE_ON | SHACF_AUTOSUGGEST_FORCE_ON);
}

void CTestWizardOutputPage::UninitializeControls(void)
{
}

void CTestWizardOutputPage::InitializeValues(void)
{
	// We'll initialize outputFileName and outputFileEncoding even if
	// the output type is not eOutput_SaveToFile (so that if a previous
	// run had store something, those will be the defaults if they want
	// to switch back to eOutput_SaveToFile).
	CString outputFileName = m_pTestWizardInfo->GetOutputFileName();
	TestWizardOutputFileEncoding outputFileEncoding = m_pTestWizardInfo->GetOutputFileEncoding();

	m_editFileName.SetWindowText(outputFileName);
	m_comboFileEncoding.SetCurSel((int)outputFileEncoding);

	TestWizardOutputType outputType = m_pTestWizardInfo->GetOutputType();

	switch(outputType)
	{
	case eOutput_SendEMail:
		m_radioSendEmail.Click();
		break;
	case eOutput_SaveToFile:
		m_radioSaveToFile.Click();
		break;
	case eOutput_Clipboard:
	default:
		m_radioCopyToClipboard.Click();
		break;
	}
}

bool CTestWizardOutputPage::StoreValues(void)
{
	if(m_radioCopyToClipboard.GetCheck() == BST_CHECKED)
	{
		m_pTestWizardInfo->SetOutputType(eOutput_Clipboard);
	}
	else if(m_radioSendEmail.GetCheck() == BST_CHECKED)
	{
		m_pTestWizardInfo->SetOutputType(eOutput_SendEMail);
	}
	else if(m_radioSaveToFile.GetCheck() == BST_CHECKED)
	{
		m_pTestWizardInfo->SetOutputType(eOutput_SaveToFile);

		CString fileName;
		int cchFileName = m_editFileName.GetWindowTextLength();
		m_editFileName.GetWindowText(fileName.GetBuffer(cchFileName + 1), cchFileName + 1);
		fileName.ReleaseBuffer(cchFileName);
		fileName.TrimLeft();
		fileName.TrimRight();

		if(fileName.GetLength() < 1)
		{
			this->MessageBox(
				_T("Please provide a file name to save the list of files."),
				_T("No File Name"), MB_OK | MB_ICONWARNING);
			m_editFileName.SetFocus();
			return false;
		}
		else
		{
			m_pTestWizardInfo->SetOutputFileName(fileName);
		}

		m_pTestWizardInfo->SetOutputFileEncoding(
			(TestWizardOutputFileEncoding) m_comboFileEncoding.GetCurSel());
	}

	return true;
}

// Overrides from base class
int CTestWizardOutputPage::OnSetActive()
{
	this->SetWizardButtons(PSWIZB_BACK | PSWIZB_NEXT);

	// 0 = allow activate
	// -1 = go back to page that was active
	// page ID = jump to page
	return 0;
}

int CTestWizardOutputPage::OnWizardNext()
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

int CTestWizardOutputPage::OnWizardBack()
{
	return m_pTestWizardInfo->FindPreviousPage(IDD);
}

void CTestWizardOutputPage::OnHelp()
{
	m_pTestWizardInfo->ShowHelp(IDD);
}
