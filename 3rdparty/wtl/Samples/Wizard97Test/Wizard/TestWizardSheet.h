
#ifndef __TestWizardSheet_h__
#define __TestWizardSheet_h__


#include "TestWizardWelcomePage.h"
#include "TestWizardPathFilterPage.h"
#include "TestWizardFilePreviewPage.h"
#include "TestWizardOutputPage.h"
#include "TestWizardCompletionPage.h"

class CTestWizardSheet :
	public CWizard97SheetImpl<CTestWizardSheet>,
	public CTestWizardInfoRef
{
protected:
// Typedefs
	typedef CTestWizardSheet thisClass;
	typedef CWizard97SheetImpl<CTestWizardSheet> baseClass;
	typedef CTestWizardInfoRef infoRefClass;

// Data members
	CTestWizardWelcomePage m_pageWelcome;
	CTestWizardPathFilterPage m_pagePathFiler;
	CTestWizardFilePreviewPage m_pageFilePreview;
	CTestWizardOutputPage m_pageOutput;
	CTestWizardCompletionPage m_pageCompletion;

public:
// Constructors
	CTestWizardSheet(CTestWizardInfo* pTestWizardInfo, UINT uStartPage = 0, HWND hWndParent = NULL);

// Message Handlers
	BEGIN_MSG_MAP(thisClass)
		MESSAGE_HANDLER(WM_HELP, OnHelp)
		CHAIN_MSG_MAP(baseClass)
	END_MSG_MAP()

	LRESULT OnHelp(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
};

#endif // __TestWizardSheet_h__
