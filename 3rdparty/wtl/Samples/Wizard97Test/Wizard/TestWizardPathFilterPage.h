
#ifndef __TestWizardPathFilterPage_h__
#define __TestWizardPathFilterPage_h__

#include "TestWizardInfo.h"

class CTestWizardPathFilterPage :
	public CWizard97InteriorPageImpl<CTestWizardPathFilterPage>,
	public CTestWizardInfoRef
{
protected:
// Typedefs
	typedef CTestWizardPathFilterPage thisClass;
	typedef CWizard97InteriorPageImpl<CTestWizardPathFilterPage> baseClass;

// Data members
	CStatic m_labelPath;
	CEdit m_editPath;
	CButton m_buttonBrowsePath;

	CButton m_radioRecurse;
	CButton m_radioNoRecurse;

	CStatic m_labelFilter;
	CButton m_radioFilterAll;
	CButton m_radioFilterCustom;
	CEdit   m_editFilterCustom;

public:
// Constructor
	CTestWizardPathFilterPage(_U_STRINGorID title = (LPCTSTR)NULL) :
		baseClass(title)
	{
		baseClass::SetHeaderTitle(_T("Path and Filter to Find Files"));
		baseClass::SetHeaderSubTitle(_T("Select the path and filter identifying a list of files."));
	}

// Message Handlers
	enum { IDD = IDD_WIZ97_PATHFILTER };
	BEGIN_MSG_MAP(thisClass)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		MESSAGE_HANDLER(WM_DESTROY, OnDestroy)

		COMMAND_HANDLER(IDC_RADIO_FILTER_ALL, BN_CLICKED, OnClickFilterAll)
		COMMAND_HANDLER(IDC_RADIO_FILTER_CUSTOM, BN_CLICKED, OnClickFilterCustom)
		COMMAND_HANDLER(IDC_BTN_BROWSEPATH, BN_CLICKED, OnClickBrowsePath)

		CHAIN_MSG_MAP(baseClass)
	END_MSG_MAP()

	LRESULT OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
	LRESULT OnDestroy(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);

	LRESULT OnClickFilterAll(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
	LRESULT OnClickFilterCustom(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
	LRESULT OnClickBrowsePath(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
	LRESULT OnClickBrowseDefaultPath(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);

// Helpers
	void InitializeControls(void);
	void UninitializeControls(void);
	void InitializeValues(void);
	bool StoreValues(void);

// Overrides from base class
	int OnSetActive();
	int OnWizardNext();
	int OnWizardBack();
	void OnHelp();
};

#endif // __TestWizardPathFilterPage_h__
