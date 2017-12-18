
#ifndef __TestWizard_h__
#define __TestWizard_h__

#include "TestWizardInfo.h"

class CTestWizard
{
protected:
// Data members
	CTestWizardInfo m_testWizardInfo;

public:
// Constructors
	CTestWizard();

// General Methods
	bool ExecuteWizard();

	void InitializeDefaultValues();
	void StoreDefaultValues();

	bool GetStringValue(CRegKeyEx& regKey, LPCTSTR valueName, CString& value);
	bool GetBoolValue(CRegKeyEx& regKey, LPCTSTR valueName, bool& value);

	bool SetStringValue(CRegKeyEx& regKey, LPCTSTR valueName, LPCTSTR value);
	bool SetBoolValue(CRegKeyEx& regKey, LPCTSTR valueName, bool value);
};

#endif // __TestWizard_h__
