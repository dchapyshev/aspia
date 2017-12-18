
#include "stdafx.h"
#include "TestWizardSheet.h"

CTestWizardSheet::CTestWizardSheet(CTestWizardInfo* pTestWizardInfo, UINT uStartPage, HWND hWndParent) :
	baseClass(_T("Test Wizard"), IDB_WIZ97_HEADER, IDB_WIZ97_WATERMARK, uStartPage, hWndParent),
	infoRefClass(pTestWizardInfo)
{
	m_pageWelcome.SetTestWizardInfo(pTestWizardInfo);
	m_pagePathFiler.SetTestWizardInfo(pTestWizardInfo);
	m_pageFilePreview.SetTestWizardInfo(pTestWizardInfo);
	m_pageOutput.SetTestWizardInfo(pTestWizardInfo);
	m_pageCompletion.SetTestWizardInfo(pTestWizardInfo);

	this->AddPage(m_pageWelcome);
	this->AddPage(m_pagePathFiler);
	this->AddPage(m_pageFilePreview);
	this->AddPage(m_pageOutput);
	this->AddPage(m_pageCompletion);
}

LRESULT CTestWizardSheet::OnHelp(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/)
{
	// We get here when the user hits F1 while on a page,
	// or uses the "What's This" button then clicks on a control.
	// We can also handle WM_HELP on the page for the cases
	// when a control on the dialog has focus.  If the page doesn't handle WM_HELP,
	// then the sheet is given a chance to handle it (and we end up here).

	LPHELPINFO helpInfo = (LPHELPINFO)lParam;
	if(helpInfo)
	{
		if(helpInfo->dwContextId != 0)
		{
			// If dwContextId is set, then the control with
			// focus has a help context ID, so we'll show context help.
			m_pTestWizardInfo->ShowContextHelp(helpInfo);
		}
		else
		{
			int currentIndex = this->GetActiveIndex();
			if(currentIndex >= 0)
			{
				int pageDialogId = this->IndexToId(currentIndex);
				if(pageDialogId != 0)
				{
					m_pTestWizardInfo->ShowHelp(pageDialogId, helpInfo->iCtrlId);
				}
			}
		}
	}

	return 0;
}
