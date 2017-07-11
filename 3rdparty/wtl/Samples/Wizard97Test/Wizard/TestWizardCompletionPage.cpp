
#include "stdafx.h"
#include "TestWizardCompletionPage.h"

LRESULT CTestWizardCompletionPage::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	this->InitializeControls();
	this->InitializeValues();

	return 1;
}

void CTestWizardCompletionPage::InitializeFont(void)
{
	// Set the font
	CLogFont logFont;
	CClientDC dc(m_hWnd);
	logFont.SetHeight(8, dc);
	::lstrcpy(logFont.lfFaceName, _T("Courier New"));

	m_fontSummary.Attach(logFont.CreateFontIndirect());
	m_editSummary.SetFont(m_fontSummary);

	// Set the tab stops to 4 characters.
	// Tab stops are in dialog units.
	TEXTMETRIC tm = {0};
	CFontHandle oldFont = dc.SelectFont(m_fontSummary);
	dc.GetTextMetrics(&tm);
	dc.SelectFont(oldFont);

	int dialogUnitsX = ::MulDiv(4, tm.tmAveCharWidth, LOWORD(GetDialogBaseUnits()));
	int tabStops = 4*dialogUnitsX;

	m_editSummary.SetTabStops(tabStops);
}

void CTestWizardCompletionPage::InitializeControls(void)
{
	CFontHandle fontExteriorPageTitleFont(baseClass::GetExteriorPageTitleFont());

	CWindow title = this->GetDlgItem(IDC_WIZ97_EXTERIOR_TITLE);
	title.SetFont(fontExteriorPageTitleFont);

	m_editSummary = this->GetDlgItem(IDC_WIZ97_SUMMARY);

	this->InitializeFont();
}

void CTestWizardCompletionPage::InitializeValues(void)
{
}

void CTestWizardCompletionPage::UpdateSummary(void)
{
	CString path = m_pTestWizardInfo->GetPath();
	bool recurse = m_pTestWizardInfo->GetRecurse();
	CString filter = m_pTestWizardInfo->GetFilter();
	TestWizardOutputType outputType = m_pTestWizardInfo->GetOutputType();

	CString text;
	text.Format(
		_T("Test Wizard: \r\n")
		_T("· Find files in the directory:\r\n")
		_T("\t%s\r\n")
		_T("· %s\r\n")
		_T("· Find files matching the filter '%s'\r\n"),
		path,
		recurse ? _T("Also search sub-directories") : _T("Only search that directory"),
		filter);
	m_editSummary.SetWindowText(text);

	CString outputDescription;
	switch(outputType)
	{
	case eOutput_SendEMail:
		outputDescription = 
			_T("· Send the file list in an e-mail\r\n")
			_T("  (using the default mail client)\r\n");
		break;
	case eOutput_SaveToFile:
		{
			CString outputFileName = m_pTestWizardInfo->GetOutputFileName();
			TestWizardOutputFileEncoding outputFileEncoding = m_pTestWizardInfo->GetOutputFileEncoding();

			outputDescription.Format(
				_T("· Save the file list to the file:\r\n")
				_T("\t%s\r\n"),
				outputFileName);
			switch(outputFileEncoding)
			{
			case eEncoding_ASCII:
				outputDescription += _T("  with ASCII encoding\r\n");
				break;
			case eEncoding_UCS2:
				outputDescription += _T("  with Unicode (UCS-2) encoding\r\n");
				break;
			case eEncoding_UTF8:
				outputDescription += _T("  with Unicode (UTF-8) encoding\r\n");
				break;
			}
		}
		break;
	case eOutput_Clipboard:
	default:
		outputDescription = _T("· Copy the file list to the clipboard\r\n");
		break;
	}
	m_editSummary.AppendText(outputDescription);
}

int CTestWizardCompletionPage::OnSetActive()
{
	this->SetWizardButtons(PSWIZB_BACK | PSWIZB_FINISH);

	// Don't remember any previous updates to the summary,
	//  and just regenerate the whole summary
	this->UpdateSummary();

	// 0 = allow activate
	// -1 = go back to page that was active
	// page ID = jump to page
	return 0;
}

int CTestWizardCompletionPage::OnWizardBack()
{
	// 0  = goto previous page
	// -1 = prevent page change
	// >0 = jump to page by dlg ID
	return m_pTestWizardInfo->FindPreviousPage(IDD);
}

INT_PTR CTestWizardCompletionPage::OnWizardFinish()
{
	// We could either do the work here, or in the place that
	// called DoModal on our Sheet (which for this example is CTestWizard).
	// The advantage of doing the work here is that we can prevent
	// the finish, and tell the user to go back and correct something.
	// The advantage of doing it in the caller of DoModal is
	// that the wizard isn't visible while the work is being done.

	// For this example, we'll do the work here (or rather call back into
	// the info class to do the work), and prevent finish if something fails.

	CWaitCursor waitCursor;

	bool success = m_pTestWizardInfo->FinishWizard(m_hWnd);

	// FALSE = allow finish
	// TRUE = prevent finish
	// HWND = prevent finish and set focus to HWND (CommCtrl 5.80 only)
	return success ? FALSE : TRUE;
}

void CTestWizardCompletionPage::OnHelp()
{
	m_pTestWizardInfo->ShowHelp(IDD);
}
