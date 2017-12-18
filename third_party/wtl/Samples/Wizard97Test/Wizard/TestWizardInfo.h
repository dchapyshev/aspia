
#ifndef __TestWizardInfo_h__
#define __TestWizardInfo_h__

static LPCTSTR s_allFiles = _T("*.*");

class ITestWizardFindFileCB
{
public:
	virtual bool OnBeginFindFiles(void) = 0;
	virtual void OnEndFindFiles(void) = 0;
	virtual bool OnBeginDirectorySearch(LPCTSTR directory) = 0;
	virtual void OnEndDirectorySearch(LPCTSTR directory) = 0;

	virtual void OnFileFound(LPCTSTR directory, LPWIN32_FIND_DATA findFileData) = 0;
};

// Enumerations
enum TestWizardOutputType
{
	eOutput_Clipboard  = 0,
	eOutput_SendEMail  = 1,
	eOutput_SaveToFile = 2,

	eOutput_First      = eOutput_Clipboard,
	eOutput_Last       = eOutput_SaveToFile,
};
enum TestWizardOutputFileEncoding
{
	eEncoding_ASCII = 0,
	eEncoding_UCS2  = 1,
	eEncoding_UTF8  = 2,

	eEncoding_First = eEncoding_ASCII,
	eEncoding_Last  = eEncoding_UTF8,
};
enum TestWizardQuerySiblingNotifiations
{
	eQuerySibling_ParametersFileChanged = 0,
};

class CTestWizardInfo
{
protected:
// Typedefs
	typedef CTestWizardInfo thisClass;

// Member variables
	CString m_path, m_filter;
	bool m_showWelcome, m_recurse;

	TestWizardOutputType m_outputType;
	CString m_outputFileName;
	TestWizardOutputFileEncoding m_outputFileEncoding;

public:
// Constructor/Destructor
	CTestWizardInfo();
	virtual ~CTestWizardInfo();

// General methods
	// Set
	bool SetShowWelcome(bool showWelcome);
	bool SetPath(LPCTSTR path);
	bool SetRecurse(bool recurse);
	bool SetFilter(LPCTSTR filter);
	bool SetOutputType(TestWizardOutputType outputType);
	bool SetOutputTypeByDisplayName(LPCTSTR typeDisplayName);
	bool SetOutputFileName(LPCTSTR outputFileName);
	bool SetOutputFileEncoding(TestWizardOutputFileEncoding outputFileEncoding);
	bool SetOutputFileEncodingByDisplayName(LPCTSTR encodingDisplayName);

	// Get
	bool GetShowWelcome(void) const;
	CString GetPath(void) const;
	bool GetRecurse(void) const;
	CString GetFilter(void) const;
	TestWizardOutputType GetOutputType(void) const;
	CString GetOutputTypeDisplayName(void) const;
	CString GetOutputFileName(void) const;
	TestWizardOutputFileEncoding GetOutputFileEncoding(void) const;
	CString GetOutputFileEncodingDisplayName(void) const;

	// Static methods
	static bool IsValidOutputType(DWORD outputType);
	static bool IsValidOutputFileEncoding(DWORD outputFileEncoding);
	static CString GetOutputTypeDisplayName(TestWizardOutputType outputType);
	static CString GetOutputFileEncodingDisplayName(TestWizardOutputFileEncoding outputFileEncoding);
	static bool GetOutputTypeForDisplayName(LPCTSTR typeDisplayName, TestWizardOutputType& outputType);
	static bool GetOutputFileEncodingForDisplayName(LPCTSTR encodingDisplayName, TestWizardOutputFileEncoding& outputFileEncoding);

	// File List
	int FindFiles(ITestWizardFindFileCB* callback) const;

	// Page Navigation
	int FindPreviousPage(int pageDialogId) const;
	int FindNextPage(int pageDialogId) const;

	// Help
	void ShowHelp(int pageDialogId, int controlId = 0);
	void ShowContextHelp(LPHELPINFO helpInfo);

	// FinishWizard
	bool FinishWizard(HWND hWndParent);

protected:
// Implementation methods

	int FindFiles(ITestWizardFindFileCB* callback, LPCTSTR directory, LPCTSTR filter, bool recurse) const;
	bool FinishWizard_CopyToClipboard(HWND hWndParent);
	bool FinishWizard_SendEMail(HWND hWndParent);
	bool FinishWizard_SaveToFile(HWND hWndParent);
};

class CTestWizardInfoRef
{
protected:
// Data members
	CTestWizardInfo* m_pTestWizardInfo;

public:
// Constructors
	CTestWizardInfoRef(CTestWizardInfo* pTestWizardInfo = NULL) : 
		m_pTestWizardInfo(pTestWizardInfo)
	{
	}

// Public methods
	CTestWizardInfo* GetTestWizardInfo(void)
	{
		return m_pTestWizardInfo;
	}
	void SetTestWizardInfo(CTestWizardInfo* pTestWizardInfo)
	{
		m_pTestWizardInfo = pTestWizardInfo;
	}

};

class CTestWizardFindFile_BuildString :
	public CTestWizardInfoRef,
	public ITestWizardFindFileCB
{
protected:
// Typedefs
	typedef CTestWizardInfoRef baseClass;

// Data members
	CString m_output;

public:
// ITestWizardFindFileCB
	virtual bool OnBeginFindFiles(void) { return true; }
	virtual void OnEndFindFiles(void) { }
	virtual bool OnBeginDirectorySearch(LPCTSTR /*directory*/) { return true; }
	virtual void OnEndDirectorySearch(LPCTSTR /*directory*/) { }

	virtual void OnFileFound(LPCTSTR directory, LPWIN32_FIND_DATA findFileData);

// Public methods
	const CString& GetOutputString() const
	{
		return m_output;
	}
};

class CTestWizardFindFile_SaveToFile :
	public CTestWizardInfoRef,
	public ITestWizardFindFileCB
{
protected:
// Typedefs
	typedef CTestWizardInfoRef baseClass;

// Data members
	CString m_outputFileName;
	HANDLE m_hFile;
	TestWizardOutputFileEncoding m_outputFileEncoding;
	bool m_succeeded;
	CString m_failureReason;

public:
// Constructor/Destructor
	CTestWizardFindFile_SaveToFile() :
		m_hFile(INVALID_HANDLE_VALUE),
		m_outputFileEncoding(eEncoding_ASCII),
		m_succeeded(false)
	{
	}

	~CTestWizardFindFile_SaveToFile()
	{
		if(m_hFile != INVALID_HANDLE_VALUE)
		{
			::CloseHandle(m_hFile);
			m_hFile = INVALID_HANDLE_VALUE;
		}
	}

// ITestWizardFindFileCB
	virtual bool OnBeginFindFiles(void);
	virtual void OnEndFindFiles(void);
	virtual bool OnBeginDirectorySearch(LPCTSTR /*directory*/) { return true; }
	virtual void OnEndDirectorySearch(LPCTSTR /*directory*/) { }

	virtual void OnFileFound(LPCTSTR directory, LPWIN32_FIND_DATA findFileData);

// Public methods
	bool Succeeded(void) const
	{
		return m_succeeded;
	}
	CString GetFailureReason(void) const
	{
		return m_failureReason;
	}
};

#endif // __TestWizardInfo_h__
