
#include "stdafx.h"
#include "TestWizard.h"
#include "TestWizardSheet.h"

LPCTSTR g_lpcstrPrefRegKey = _T("Software\\Microsoft\\WTL Samples\\Wizard97Test");

CTestWizard::CTestWizard()
{
}

bool CTestWizard::ExecuteWizard()
{
	// You could also pass in parameters here
	// (or something generic like a name/value map).

	bool success = false;

	this->InitializeDefaultValues();

	CTestWizardSheet wizard(&m_testWizardInfo);
	INT_PTR result = wizard.DoModal();
	if(result == IDOK)
	{
		// You could either do the work here, or in
		// OnWizardFinish in the completion page class.
		success = true;

		this->StoreDefaultValues();
	}

	return success;
}

void CTestWizard::InitializeDefaultValues()
{
	bool showWelcome = true;
	CString defaultPath;
	bool defaultRecurse = true;
	CString defaultFilter;
	TestWizardOutputType outputType = eOutput_Clipboard;
	CString outputFileName;
	TestWizardOutputFileEncoding outputFileEncoding = eEncoding_ASCII;

	CRegKeyEx regKey;
	LONG result = regKey.Open(HKEY_CURRENT_USER, g_lpcstrPrefRegKey);
	if(result == ERROR_SUCCESS)
	{
		this->GetBoolValue(regKey, _T("showWelcome"), showWelcome);
		this->GetStringValue(regKey, _T("path"), defaultPath);
		this->GetBoolValue(regKey, _T("recurse"), defaultRecurse);
		this->GetStringValue(regKey, _T("filter"), defaultFilter);

		CString outputTypeDisplayName;
		if(this->GetStringValue(regKey, _T("outputType"), outputTypeDisplayName))
		{
			CTestWizardInfo::GetOutputTypeForDisplayName(outputTypeDisplayName, outputType);

			// NOTE: We could have it so that "outputFileName" and "outputFileEncoding"
			//  were only looked for if the last output type were eOutput_SaveToFile.
			//  However, in case a previous run had done eOutput_SaveToFile, we'll
			//  load up what's there.

			//if(outputType == eOutput_SaveToFile)

			this->GetStringValue(regKey, _T("outputFileName"), outputFileName);

			CString outputFileEncodingDisplayName;
			if(this->GetStringValue(regKey, _T("outputFileEncoding"), outputFileEncodingDisplayName))
			{
				CTestWizardInfo::GetOutputFileEncodingForDisplayName(outputFileEncodingDisplayName, outputFileEncoding);
			}
		}
	}
	regKey.Close();

	if(defaultPath.IsEmpty())
	{
		::GetCurrentDirectory(MAX_PATH, defaultPath.GetBuffer(MAX_PATH+1));
		defaultPath.ReleaseBuffer();
	}
	if(defaultFilter.IsEmpty())
	{
		defaultFilter = _T("*.*");
	}

	m_testWizardInfo.SetShowWelcome(showWelcome);
	m_testWizardInfo.SetPath(defaultPath);
	m_testWizardInfo.SetRecurse(defaultRecurse);
	m_testWizardInfo.SetFilter(defaultFilter);

	m_testWizardInfo.SetOutputType(outputType);

	m_testWizardInfo.SetOutputFileName(outputFileName);
	m_testWizardInfo.SetOutputFileEncoding(outputFileEncoding);
}

void CTestWizard::StoreDefaultValues()
{
	bool showWelcome = m_testWizardInfo.GetShowWelcome();
	CString path = m_testWizardInfo.GetPath();
	bool recurse = m_testWizardInfo.GetRecurse();
	CString filter = m_testWizardInfo.GetFilter();
	TestWizardOutputType outputType = m_testWizardInfo.GetOutputType();
	CString outputTypeDisplayName = m_testWizardInfo.GetOutputTypeDisplayName();

	CRegKeyEx regKey;
	LONG result = regKey.Open(HKEY_CURRENT_USER, g_lpcstrPrefRegKey);
	if(result != ERROR_SUCCESS)
	{
		result = regKey.Create(HKEY_CURRENT_USER, g_lpcstrPrefRegKey);
	}
	if(result == ERROR_SUCCESS)
	{
		this->SetBoolValue(regKey, _T("showWelcome"), showWelcome);
		this->SetStringValue(regKey, _T("path"), path);
		this->SetBoolValue(regKey, _T("recurse"), recurse);
		this->SetStringValue(regKey, _T("filter"), filter);

		// NOTE: For enumerations, we could either store the display name
		//  or the enumeration value.  Which ever one you choose to store,
		//  for future versions you either need to ensure that value 
		//  never changes, or you need to do a conversion.  To do a conversion,
		//  one way is to store a "schema version" number so that readers
		//  know what you're written.
		//
		//  We'll choose to store the display name so that the enumeration
		//  values can change (to change their order perhaps).
		this->SetStringValue(regKey, _T("outputType"), outputTypeDisplayName);
		if(outputType == eOutput_SaveToFile)
		{
			CString outputFileName = m_testWizardInfo.GetOutputFileName();
			CString outputFileEncodingDisplayName = m_testWizardInfo.GetOutputFileEncodingDisplayName();

			this->SetStringValue(regKey, _T("outputFileName"), outputFileName);
			this->SetStringValue(regKey, _T("outputFileEncoding"), outputFileEncodingDisplayName);
		}
		else
		{
			// Since "outputFileName" and "outputFileEncoding" are used with
			//  eOutput_SaveToFile, we could technically delete them if a previous
			//  run had stored them.  But we'll leave them in as defaults
			//  for future runs in case they switch back.
			//regKey.DeleteValue(_T("outputFileName"));
			//regKey.DeleteValue(_T("outputFileEncoding"));
		}
	}
	regKey.Close();
}

bool CTestWizard::GetStringValue(CRegKeyEx& regKey, LPCTSTR valueName, CString& value)
{
	bool success = false;

	DWORD cchValue = 0;
	LONG result = regKey.QueryStringValue(valueName, NULL, &cchValue);
	if((result == ERROR_SUCCESS) && (cchValue > 0))
	{
		regKey.QueryStringValue(valueName, value.GetBuffer(cchValue+1), &cchValue);
		value.ReleaseBuffer();
		success = true;
	}

	return success;
}

bool CTestWizard::GetBoolValue(CRegKeyEx& regKey, LPCTSTR valueName, bool& value)
{
	bool success = false;

	DWORD dwValue = 0;
	LONG result = regKey.QueryDWORDValue(valueName, dwValue);
	if(result == ERROR_SUCCESS)
	{
		value = (dwValue != 0);
		success = true;
	}

	return success;
}

bool CTestWizard::SetStringValue(CRegKeyEx& regKey, LPCTSTR valueName, LPCTSTR value)
{
	return (ERROR_SUCCESS == regKey.SetStringValue(valueName, value, REG_SZ));
}

bool CTestWizard::SetBoolValue(CRegKeyEx& regKey, LPCTSTR valueName, bool value)
{
	return (ERROR_SUCCESS == regKey.SetDWORDValue(valueName, (value ? 1 : 0)));
}

