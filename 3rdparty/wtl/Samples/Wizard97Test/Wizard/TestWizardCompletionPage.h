
#ifndef __TestWizardCompletionPage_h__
#define __TestWizardCompletionPage_h__

#include "TestWizardInfo.h"

class CTestWizardCompletionPage :
	public CWizard97ExteriorPageImpl<CTestWizardCompletionPage>,
	public CTestWizardInfoRef
{
protected:
// Typedefs
	typedef CTestWizardCompletionPage thisClass;
	typedef CWizard97ExteriorPageImpl<CTestWizardCompletionPage> baseClass;

// Data members
	CFont m_fontSummary;
	CRichEditCtrl m_editSummary;

public:
// Constructors
	CTestWizardCompletionPage(_U_STRINGorID title = (LPCTSTR)NULL) :
		baseClass(title)
	{ }

// Message Handlers
	enum { IDD = IDD_WIZ97_COMPLETION };
	BEGIN_MSG_MAP(thisClass)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)

		CHAIN_MSG_MAP(baseClass)
	END_MSG_MAP()

	LRESULT OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);

// Helpers
	void InitializeFont(void);
	void InitializeControls(void);
	void InitializeValues(void);
	void UpdateSummary(void);

// Overrides from base class
	int OnSetActive();
	int OnWizardBack();
	int OnWizardFinish();
	void OnHelp();
};

#endif // __TestWizardCompletionPage_h__
