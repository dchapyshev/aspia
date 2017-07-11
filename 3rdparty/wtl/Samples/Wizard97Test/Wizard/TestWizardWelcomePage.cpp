
#include "stdafx.h"
#include "TestWizardWelcomePage.h"

LRESULT CTestWizardWelcomePage::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	this->InitializeControls();
	this->InitializeValues();

	return 1;
}

void CTestWizardWelcomePage::InitializeControls(void)
{
	CFontHandle fontExteriorPageTitleFont(baseClass::GetExteriorPageTitleFont());
	CFontHandle fontBulletFont(baseClass::GetBulletFont());

	CWindow title = this->GetDlgItem(IDC_WIZ97_EXTERIOR_TITLE);
	CWindow bullet1 = this->GetDlgItem(IDC_WIZ97_BULLET1);
	CWindow bullet2 = this->GetDlgItem(IDC_WIZ97_BULLET2);
	CWindow bullet3 = this->GetDlgItem(IDC_WIZ97_BULLET3);
	CWindow bullet4 = this->GetDlgItem(IDC_WIZ97_BULLET4);
	m_buttonSkipWelcome = this->GetDlgItem(IDC_WIZ97_WELCOME_NOTAGAIN);

	title.SetFont(fontExteriorPageTitleFont);
	bullet1.SetFont(fontBulletFont);
	bullet2.SetFont(fontBulletFont);
	bullet3.SetFont(fontBulletFont);
	bullet4.SetFont(fontBulletFont);
}

void CTestWizardWelcomePage::InitializeValues(void)
{
	bool showWelcome = m_pTestWizardInfo->GetShowWelcome();
	m_buttonSkipWelcome.SetCheck(showWelcome ? BST_UNCHECKED : BST_CHECKED);
}

bool CTestWizardWelcomePage::StoreValues(void)
{
	m_pTestWizardInfo->SetShowWelcome(m_buttonSkipWelcome.GetCheck() == BST_UNCHECKED);
	return true;
}

// Overrides from base class
int CTestWizardWelcomePage::OnSetActive()
{
	this->SetWizardButtons(PSWIZB_NEXT);

	// 0 = allow activate
	// -1 = go back to page that was active
	// page ID = jump to page
	int result = 0;

	if(m_allowWelcomeToHide)
	{
		// Have it so that the welcome page is only hidden on
		// the first access, but is available if the user goes
		// "back" to visit it.
		m_allowWelcomeToHide = false;
		if(m_buttonSkipWelcome.GetCheck() == BST_CHECKED)
		{
			result = IDD_WIZ97_PATHFILTER;
		}
	}

	return result;
}

int CTestWizardWelcomePage::OnWizardNext()
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

void CTestWizardWelcomePage::OnHelp()
{
	m_pTestWizardInfo->ShowHelp(IDD);
}
