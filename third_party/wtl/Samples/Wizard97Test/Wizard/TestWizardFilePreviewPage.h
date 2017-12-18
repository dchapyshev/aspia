
#ifndef __TestWizardFilePreviewPage_h__
#define __TestWizardFilePreviewPage_h__

#include "TestWizardInfo.h"

///////////////////////////////////////////////////////////////////////////////
// CFileListViewCtrl - a sortable list view of the resulting files

typedef CWinTraits<
			WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
			LVS_REPORT | LVS_SHOWSELALWAYS | LVS_SHAREIMAGELISTS,
			WS_EX_CLIENTEDGE> CFileListViewCtrlWinTraits;

class CFileListViewCtrl :
	public CSortListViewCtrlImpl<CFileListViewCtrl, CListViewCtrl, CFileListViewCtrlWinTraits>,
	public ITestWizardFindFileCB
{
protected:
// Typedefs
	typedef CFileListViewCtrl thisClass;
	typedef CSortListViewCtrlImpl<CFileListViewCtrl, CListViewCtrl, CFileListViewCtrlWinTraits> baseClass;

public:
// Enumerations
	enum ListColumnIndex
	{
		ListColumn_Name         = 0,
		ListColumn_Folder       = 1,
		ListColumn_LastModified = 2,
		ListColumn_Size         = 3,
		ListColumn_SizeBytes    = 4,
		ListColumn_FullPath     = 5,
	};

// Message Handling
	DECLARE_WND_SUPERCLASS(_T("FileListView"), CListViewCtrl::GetWndClassName())

	BEGIN_MSG_MAP(thisClass)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
		MESSAGE_HANDLER(WM_CONTEXTMENU, OnContextMenu)

		CHAIN_MSG_MAP(baseClass)
		DEFAULT_REFLECTION_HANDLER()
	END_MSG_MAP()

	LRESULT OnCreate(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
	LRESULT OnDestroy(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
	LRESULT OnContextMenu(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);

// Helpers
	void Initialize(void);
	void InitializeListColumns(void);
	void Uninitialize(void);

// Overrides for CWindowImpl
	BOOL SubclassWindow(HWND hWnd);
	HWND UnsubclassWindow(BOOL bForce = FALSE);

// Overrides for CSortListViewImpl
	int CompareItemsCustom(LVCompareParam* pItem1, LVCompareParam* pItem2, int iSortCol);

// ITestWizardFindFileCB
	virtual bool OnBeginFindFiles(void) { return true; }
	virtual void OnEndFindFiles(void) { }
	virtual bool OnBeginDirectorySearch(LPCTSTR /*directory*/) { return true; }
	virtual void OnEndDirectorySearch(LPCTSTR /*directory*/) { }
	virtual void OnFileFound(LPCTSTR directory, LPWIN32_FIND_DATA findFileData);

// Methods
	int AddFile(LPCTSTR fileFullPath);
	int AddFile(LPCTSTR directory, LPCTSTR fileSpec, LPCTSTR fileFullPath, FILETIME lastWriteTimeUTC, ULONGLONG fileSize);
	void AutoResizeColumns(void);
	void ClearSortHeaderBitmap(void);
};


///////////////////////////////////////////////////////////////////////////////
// CTestWizardFilePreviewPage - Wizard page to preview the files located by the path/filter

class CTestWizardFilePreviewPage :
	public CWizard97InteriorPageImpl<CTestWizardFilePreviewPage>,
	public CTestWizardInfoRef
{
protected:
// Typedefs
	typedef CTestWizardFilePreviewPage thisClass;
	typedef CWizard97InteriorPageImpl<CTestWizardFilePreviewPage> baseClass;

// Data members
	CFileListViewCtrl m_listFiles;

	CButton m_buttonPreview;

public:
// Constructor
	CTestWizardFilePreviewPage(_U_STRINGorID title = (LPCTSTR)NULL) :
		baseClass(title)
	{
		baseClass::SetHeaderTitle(_T("Preview File List"));
		baseClass::SetHeaderSubTitle(_T("Preview the list of files identified by the path and filter."));
	}

// Message Handlers
	enum { IDD = IDD_WIZ97_FILEPREVIEW };
	BEGIN_MSG_MAP(thisClass)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		MESSAGE_HANDLER(WM_DESTROY, OnDestroy)

		COMMAND_HANDLER(IDC_BTN_PREVIEW, BN_CLICKED, OnClickPreview)

		CHAIN_MSG_MAP(baseClass)
		REFLECT_NOTIFICATIONS_ID_FILTERED(IDC_LIST_FILES)
	END_MSG_MAP()

	LRESULT OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
	LRESULT OnDestroy(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);

	LRESULT OnClickPreview(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);

// Helpers
	void InitializeControls(void);
	//void InitializeControlSizes(void);
	void UninitializeControls(void);
	void InitializeValues(void);
	bool StoreValues(void);

	void UpdateFileList();

// Overrides from base class
	int OnSetActive();
	int OnWizardNext();
	int OnWizardBack();
	void OnHelp();

};

#endif // __TestWizardFilePreviewPage_h__
