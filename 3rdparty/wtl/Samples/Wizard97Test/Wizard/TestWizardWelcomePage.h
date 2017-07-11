
#ifndef __TestWizardWelcomePage_h__
#define __TestWizardWelcomePage_h__

#include "TestWizardInfo.h"

class CTestWizardWelcomePage :
	public CWizard97ExteriorPageImpl<CTestWizardWelcomePage>,
	public CTestWizardInfoRef
{
protected:
// Typedefs
	typedef CTestWizardWelcomePage thisClass;
	typedef CWizard97ExteriorPageImpl<CTestWizardWelcomePage> baseClass;

// Data members
	CButton m_buttonSkipWelcome;
	bool m_allowWelcomeToHide;

public:
// Constructors
	CTestWizardWelcomePage(_U_STRINGorID title = (LPCTSTR)NULL) :
		baseClass(title),
		m_allowWelcomeToHide(true)
	{ }

// Message Handlers
	enum { IDD = IDD_WIZ97_WELCOME };
	BEGIN_MSG_MAP(thisClass)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)

		CHAIN_MSG_MAP(baseClass)
	END_MSG_MAP()

	LRESULT OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);

// Helper methods
	void InitializeControls(void);
	void InitializeValues(void);
	bool StoreValues(void);

// Overrides from base class
	int OnSetActive();
	int OnWizardNext();
	void OnHelp();
};

#endif // __TestWizardWelcomePage_h__
