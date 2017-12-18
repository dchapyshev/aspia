
#ifndef __TestWizardOutputPage_h__
#define __TestWizardOutputPage_h__

#include "TestWizardInfo.h"

class CTestWizardOutputPage :
	public CWizard97InteriorPageImpl<CTestWizardOutputPage>,
	public CTestWizardInfoRef
{
protected:
// Typedefs
	typedef CTestWizardOutputPage thisClass;
	typedef CWizard97InteriorPageImpl<CTestWizardOutputPage> baseClass;

// Data members
	CButton m_radioCopyToClipboard;
	CButton m_radioSendEmail;
	CButton m_radioSaveToFile;

	CStatic m_labelSaveFileName;
	CEdit   m_editFileName;
	CButton m_buttonBrowseFile;
	CStatic m_labelFileEncoding;
	CComboBox m_comboFileEncoding;

public:
// Constructor
	CTestWizardOutputPage(_U_STRINGorID title = (LPCTSTR)NULL) :
		baseClass(title)
	{
		baseClass::SetHeaderTitle(_T("Output File List"));
		baseClass::SetHeaderSubTitle(_T("Please choose how to output the file list."));
	}

// Message Handlers
	enum { IDD = IDD_WIZ97_OUTPUT };
	BEGIN_MSG_MAP(thisClass)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		MESSAGE_HANDLER(WM_DESTROY, OnDestroy)

		COMMAND_HANDLER(IDC_RADIO_COPYTOCLIPBOARD, BN_CLICKED, OnClickCopyToClipboard)
		COMMAND_HANDLER(IDC_RADIO_SENDEMAIL, BN_CLICKED, OnClickSendEmail)
		COMMAND_HANDLER(IDC_RADIO_SAVETOFILE, BN_CLICKED, OnClickSaveToFile)
		COMMAND_HANDLER(IDC_BTN_FILEBROWSE, BN_CLICKED, OnClickBrowseFileName)

		CHAIN_MSG_MAP(baseClass)
	END_MSG_MAP()

	LRESULT OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
	LRESULT OnDestroy(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);

	LRESULT OnClickCopyToClipboard(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
	LRESULT OnClickSendEmail(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
	LRESULT OnClickSaveToFile(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
	LRESULT OnClickBrowseFileName(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);

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

#endif // __TestWizardOutputPage_h__
