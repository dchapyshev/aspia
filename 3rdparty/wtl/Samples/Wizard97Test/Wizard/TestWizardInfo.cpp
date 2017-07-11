
#include "stdafx.h"
#include "TestWizardInfo.h"

#include <HtmlHelp.h>
#include <mapi.h>

// NOTE: These "Display Name" string arrays depends on the enumeration
//  value corresponding to the index into the array.
static LPCTSTR s_testWizardOutputTypeDisplayName[eOutput_Last+1] = {
	_T("Copy to Clipboard"),
	_T("Send e-mail"),
	_T("Save to file"),
};

static LPCTSTR s_testWizardOutputFileEncodingDisplayName[eEncoding_Last+1] = {
	_T("ASCII"),
	_T("Unicode (UCS-2)"),
	_T("Unicode (UTF-8)"),
};

CTestWizardInfo::CTestWizardInfo() :
	m_showWelcome(false),
	m_recurse(false),
	m_filter(s_allFiles),
	m_outputFileEncoding(eEncoding_ASCII)
{
}

CTestWizardInfo::~CTestWizardInfo()
{
}

// Set
bool CTestWizardInfo::SetShowWelcome(bool showWelcome)
{
	m_showWelcome = showWelcome;
	return (m_showWelcome == showWelcome);
}

bool CTestWizardInfo::SetPath(LPCTSTR path)
{
	m_path = path;
	return (m_path == path);
}

bool CTestWizardInfo::SetRecurse(bool recurse)
{
	m_recurse = recurse;
	return (m_recurse == recurse);
}

bool CTestWizardInfo::SetFilter(LPCTSTR filter)
{
	m_filter = filter;
	return (m_filter == filter);
}

bool CTestWizardInfo::SetOutputType(TestWizardOutputType outputType)
{
	m_outputType = outputType;
	return (m_outputType == outputType);
}

bool CTestWizardInfo::SetOutputTypeByDisplayName(LPCTSTR typeDisplayName)
{
	return thisClass::GetOutputTypeForDisplayName(typeDisplayName, m_outputType);
}

bool CTestWizardInfo::SetOutputFileName(LPCTSTR outputFileName)
{
	m_outputFileName = outputFileName;
	return (m_outputFileName == outputFileName);
}

bool CTestWizardInfo::SetOutputFileEncoding(TestWizardOutputFileEncoding outputFileEncoding)
{
	m_outputFileEncoding = outputFileEncoding;
	return (m_outputFileEncoding == outputFileEncoding);
}

bool CTestWizardInfo::SetOutputFileEncodingByDisplayName(LPCTSTR encodingDisplayName)
{
	return thisClass::GetOutputFileEncodingForDisplayName(encodingDisplayName, m_outputFileEncoding);
}

// Get
bool CTestWizardInfo::GetShowWelcome(void) const
{
	return m_showWelcome;
}

CString CTestWizardInfo::GetPath(void) const
{
	return m_path;
}

bool CTestWizardInfo::GetRecurse(void) const
{
	return m_recurse;
}

CString CTestWizardInfo::GetFilter(void) const
{
	return m_filter;
}

TestWizardOutputType CTestWizardInfo::GetOutputType(void) const
{
	return m_outputType;
}

CString CTestWizardInfo::GetOutputTypeDisplayName(void) const
{
	return thisClass::GetOutputTypeDisplayName(m_outputType);
}

CString CTestWizardInfo::GetOutputFileName(void) const
{
	return m_outputFileName;
}

TestWizardOutputFileEncoding CTestWizardInfo::GetOutputFileEncoding(void) const
{
	return m_outputFileEncoding;
}

CString CTestWizardInfo::GetOutputFileEncodingDisplayName(void) const
{
	return thisClass::GetOutputFileEncodingDisplayName(m_outputFileEncoding);
}

// Static methods
bool CTestWizardInfo::IsValidOutputType(DWORD outputType)
{
	return ((outputType >= (DWORD)eOutput_First) && (outputType <= (DWORD)eOutput_Last));
}

bool CTestWizardInfo::IsValidOutputFileEncoding(DWORD outputFileEncoding)
{
	return ((outputFileEncoding >= (DWORD)eEncoding_First) && (outputFileEncoding <= (DWORD)eEncoding_Last));
}

CString CTestWizardInfo::GetOutputTypeDisplayName(TestWizardOutputType outputType)
{
	CString typeDisplayName;

	ATLASSERT(thisClass::IsValidOutputType((DWORD)outputType));
	if(thisClass::IsValidOutputType((DWORD)outputType))
	{
		typeDisplayName = s_testWizardOutputTypeDisplayName[outputType];
	}

	return typeDisplayName;
}

CString CTestWizardInfo::GetOutputFileEncodingDisplayName(TestWizardOutputFileEncoding outputFileEncoding)
{
	CString encodingDisplayName;

	ATLASSERT(thisClass::IsValidOutputFileEncoding((DWORD)outputFileEncoding));
	if(thisClass::IsValidOutputFileEncoding((DWORD)outputFileEncoding))
	{
		encodingDisplayName = s_testWizardOutputFileEncodingDisplayName[outputFileEncoding];
	}

	return encodingDisplayName;
}

bool CTestWizardInfo::GetOutputTypeForDisplayName(LPCTSTR typeDisplayName, TestWizardOutputType& outputType)
{
	bool success = false;

	ATLASSERT(typeDisplayName != NULL);
	if(typeDisplayName != NULL)
	{
		for(int i=(int)eOutput_First; i<=(int)eOutput_Last && !success; ++i)
		{
			if(::lstrcmpi(typeDisplayName, s_testWizardOutputTypeDisplayName[i]) == 0)
			{
				outputType = (TestWizardOutputType)i;
				success = true;
			}
		}
	}

	return success;
}

bool CTestWizardInfo::GetOutputFileEncodingForDisplayName(LPCTSTR encodingDisplayName, TestWizardOutputFileEncoding& outputFileEncoding)
{
	bool success = false;

	ATLASSERT(encodingDisplayName != NULL);
	if(encodingDisplayName != NULL)
	{
		for(int i=(int)eEncoding_First; i<=(int)eEncoding_Last && !success; ++i)
		{
			if(::lstrcmpi(encodingDisplayName, s_testWizardOutputFileEncodingDisplayName[i]) == 0)
			{
				outputFileEncoding = (TestWizardOutputFileEncoding)i;
				success = true;
			}
		}
	}

	return success;
}

// File List
int CTestWizardInfo::FindFiles(ITestWizardFindFileCB* callback) const
{
	ATLASSERT(callback != NULL);
	if(callback == NULL)
	{
		return 0;
	}

	int fileCount = 0;

	bool success = callback->OnBeginFindFiles();
	if(success)
	{
		fileCount = this->FindFiles(callback, m_path, m_filter, m_recurse);

		callback->OnEndFindFiles();
	}

	return fileCount;
}

int CTestWizardInfo::FindFiles(ITestWizardFindFileCB* callback, LPCTSTR directory, LPCTSTR filter, bool recurse) const
{
	int fileCount = 0;

	bool success = callback->OnBeginDirectorySearch(directory);
	if(success)
	{
		// First search current directory for files specified by the filter

		TCHAR directoryWithFilter[MAX_PATH] = {0};
		::PathCombine(directoryWithFilter, directory, filter);

		WIN32_FIND_DATA findFileData = {0};
		HANDLE hFindFile = ::FindFirstFile(directoryWithFilter, &findFileData);
		if(hFindFile != INVALID_HANDLE_VALUE)
		{
			do
			{
				if(FILE_ATTRIBUTE_DIRECTORY != (findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
				{
					fileCount += 1;

					callback->OnFileFound(directory, &findFileData);
				}
			} while(TRUE == ::FindNextFile(hFindFile, &findFileData));

			::FindClose(hFindFile);
			hFindFile = NULL;
		}

		// Then recurse sub-directories if appropriate
		if(recurse)
		{
			::PathCombine(directoryWithFilter, directory, _T("*"));

			HANDLE hFindDirectory = NULL;
			WIN32_FIND_DATA findDirectoryData = {0};
			hFindDirectory = ::FindFirstFileEx(directoryWithFilter, FindExInfoStandard, &findDirectoryData, FindExSearchLimitToDirectories, NULL, 0);
			if(hFindDirectory != INVALID_HANDLE_VALUE)
			{
				TCHAR subDirectory[MAX_PATH] = {0};

				do
				{
					if(	FILE_ATTRIBUTE_DIRECTORY == (findDirectoryData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
						::lstrcmp(_T("."), findDirectoryData.cFileName) != 0 &&
						::lstrcmp(_T(".."), findDirectoryData.cFileName) != 0)
					{
						::PathCombine(subDirectory, directory, findDirectoryData.cFileName);

						fileCount += this->FindFiles(callback, subDirectory, filter, recurse);
					}
				} while(TRUE == ::FindNextFile(hFindDirectory, &findDirectoryData));

				::FindClose(hFindDirectory);
				hFindDirectory = NULL;
			}
		}

		callback->OnEndDirectorySearch(directory);
	}

	return fileCount;
}

int CTestWizardInfo::FindPreviousPage(int /*pageDialogId*/) const
{
	// 0  = goto previous page
	// -1 = prevent page change
	// >0 = jump to page by dlg ID
	int page = 0;

	//switch(pageDialogId)
	//{
	//case IDD_WIZ97_WELCOME:
	//	break;
	//case IDD_WIZ97_PATHFILTER:
	//	break;
	//case IDD_WIZ97_FILEPREVIEW:
	//	break;
	//case IDD_WIZ97_OUTPUT:
	//	break;
	//case IDD_WIZ97_COMPLETION:
	//	break;
	//}

	return page;
}

int CTestWizardInfo::FindNextPage(int /*pageDialogId*/) const
{
	// 0  = goto next page
	// -1 = prevent page change
	// >0 = jump to page by dlg ID
	int page = 0;

	//switch(pageDialogId)
	//{
	//case IDD_WIZ97_WELCOME:
	//	break;
	//case IDD_WIZ97_PATHFILTER:
	//	break;
	//case IDD_WIZ97_FILEPREVIEW:
	//	break;
	//case IDD_WIZ97_OUTPUT:
	//	break;
	//case IDD_WIZ97_COMPLETION:
	//	break;
	//}

	return page;
}

void CTestWizardInfo::ShowHelp(int pageDialogId, int /*controlId*/)
{
	LPCTSTR topicPath = NULL;

	switch(pageDialogId)
	{
	case IDD_WIZ97_WELCOME:
		topicPath = _T("/TestWizard_Welcome.html");
		break;
	case IDD_WIZ97_PATHFILTER:
		topicPath = _T("/TestWizard_PathFilter.html");
		break;
	case IDD_WIZ97_FILEPREVIEW:
		topicPath = _T("/TestWizard_PreviewFileList.html");
		break;
	case IDD_WIZ97_OUTPUT:
		topicPath = _T("/TestWizard_Output.html");
		break;
	case IDD_WIZ97_COMPLETION:
		topicPath = _T("/TestWizard_Completion.html");
		break;
	}

	TCHAR helpFileLink[2*MAX_PATH+1] = {0};
	::GetModuleFileName(NULL, helpFileLink, 2*MAX_PATH);
	::PathRenameExtension(helpFileLink, _T(".chm"));

	UINT command = HH_DISPLAY_TOC;
	if(topicPath)
	{
		command = HH_DISPLAY_TOPIC;
		::lstrcat(helpFileLink, _T("::"));
		::lstrcat(helpFileLink, topicPath);
	}

	// Add the window name
	::lstrcat(helpFileLink, _T(">$global_main"));

	// In real code, you'd probably want the "main frame" to be hWndHelpParent.
	HWND hWndHelpParent = ::GetActiveWindow();

	HWND hWndHelp = ::HtmlHelp(hWndHelpParent, helpFileLink, command, NULL);
	ATLASSERT(hWndHelp != NULL);

	if(hWndHelp != NULL && topicPath != NULL)
	{
		// NOTE: It'd probably be better to use the notification
		//  HHN_NAVCOMPLETE to know when we can synchronize
		::Sleep(200);
		::PostMessage(hWndHelp, WM_COMMAND, MAKEWPARAM(IDTB_SYNC, 0), 0);
	}
}

void CTestWizardInfo::ShowContextHelp(LPHELPINFO helpInfo)
{
	TCHAR helpFileLink[2*MAX_PATH+1] = {0};
	::GetModuleFileName(NULL, helpFileLink, 2*MAX_PATH);
	::PathRenameExtension(helpFileLink, _T(".chm"));

	::lstrcat(helpFileLink, _T("::/Context.txt"));

	DWORD idList[3] = { helpInfo->iCtrlId, (DWORD)helpInfo->dwContextId, 0};

	// For more control on the appearance, we could use HH_DISPLAY_TEXT_POPUP
	HWND hWndHelp = ::HtmlHelp((HWND)helpInfo->hItemHandle, helpFileLink, HH_TP_HELP_WM_HELP, (DWORD_PTR)idList);
	ATLASSERT(hWndHelp != NULL);
	if(hWndHelp == NULL)
	{
		::MessageBox(::GetActiveWindow(), _T("Unable to show context help"), _T("Error"), MB_OK | MB_ICONERROR);
	}
}

bool CTestWizardInfo::FinishWizard(HWND hWndParent)
{
	bool success = false;

	switch(m_outputType)
	{
	case eOutput_SendEMail:
		success = this->FinishWizard_SendEMail(hWndParent);
		break;
	case eOutput_SaveToFile:
		success = this->FinishWizard_SaveToFile(hWndParent);
		break;
	case eOutput_Clipboard:
	default:
		success = this->FinishWizard_CopyToClipboard(hWndParent);
		break;
	}

	return success;
}

bool CTestWizardInfo::FinishWizard_CopyToClipboard(HWND hWndParent)
{
	bool success = false;

	CTestWizardFindFile_BuildString callback;
	callback.SetTestWizardInfo(this);

	this->FindFiles(&callback);

	const CString& output = callback.GetOutputString();

	size_t cbOutput = (output.GetLength() + 1) * sizeof(TCHAR);
	HGLOBAL hClipboardData = ::GlobalAlloc((GMEM_MOVEABLE|GMEM_ZEROINIT), cbOutput);
	if(hClipboardData != NULL)
	{
		LPVOID pClipboardData = ::GlobalLock(hClipboardData);
		LPCTSTR sourceData = output.operator LPCTSTR();
		::CopyMemory(pClipboardData, sourceData, cbOutput);
		::GlobalUnlock(hClipboardData);

		BOOL openedClipboard = ::OpenClipboard(hWndParent);
		if(openedClipboard)
		{
			::EmptyClipboard();

#ifdef _UNICODE
			HANDLE handle = ::SetClipboardData(CF_UNICODETEXT, hClipboardData);
			// Let the OS synthesize CF_TEXT
#else
			HANDLE handle = ::SetClipboardData(CF_TEXT, hClipboardData);
			// Let the OS synthesize CF_UNICODETEXT
#endif

			success = (handle != NULL);

			::CloseClipboard();
		}
	}

	if(!success)
	{
		::MessageBox(hWndParent,
			_T("Failed to copy the file list to the clipboard"),
			_T("Error"), MB_OK | MB_ICONERROR);
	}

	return success;
}

bool CTestWizardInfo::FinishWizard_SendEMail(HWND hWndParent)
{
	bool success = false;

	CTestWizardFindFile_BuildString callback;
	callback.SetTestWizardInfo(this);

	this->FindFiles(&callback);

	const CString& output = callback.GetOutputString();

	HMODULE hMapi = LoadLibrary(_T("MAPI32.DLL"));
	if(hMapi)
	{
		LPMAPISENDMAIL pfnMAPISendMail = (LPMAPISENDMAIL) ::GetProcAddress(hMapi, "MAPISendMail");
		if(pfnMAPISendMail)
		{
			USES_CONVERSION;
			MapiMessage message = {0};
			message.lpszNoteText = (LPSTR)T2CA(output);

			ULONG result = pfnMAPISendMail(NULL, NULL, &message, MAPI_DIALOG, 0);
			success = (result == SUCCESS_SUCCESS) || (result == MAPI_USER_ABORT);
		}
		
		::FreeLibrary(hMapi);
		hMapi = NULL;
	}

	if(!success)
	{
		::MessageBox(hWndParent,
			_T("Failed to send the file list via e-mail"),
			_T("Error"), MB_OK | MB_ICONERROR);
	}

	return success;
}

bool CTestWizardInfo::FinishWizard_SaveToFile(HWND hWndParent)
{
	CTestWizardFindFile_SaveToFile callback;
	callback.SetTestWizardInfo(this);

	this->FindFiles(&callback);

	bool success = callback.Succeeded();

	if(!success)
	{
		CString failureReason = callback.GetFailureReason();
		if(failureReason.GetLength() < 1)
		{
			failureReason.Format(
				_T("Failed to save the file list to the file\r\n%s"),
				m_outputFileName);
		}

		::MessageBox(hWndParent,
			failureReason,
			_T("Error"), MB_OK | MB_ICONERROR);
	}

	return success;
}

void CTestWizardFindFile_BuildString::OnFileFound(LPCTSTR directory, LPWIN32_FIND_DATA findFileData)
{
	TCHAR fullFilePath[MAX_PATH+3] = {0};
	::PathCombine(fullFilePath, directory, findFileData->cFileName);
	::lstrcat(fullFilePath, _T("\r\n"));

	m_output += fullFilePath;
}

bool CTestWizardFindFile_SaveToFile::OnBeginFindFiles(void)
{
	m_outputFileName = m_pTestWizardInfo->GetOutputFileName();
	m_outputFileEncoding = m_pTestWizardInfo->GetOutputFileEncoding();

	m_hFile = ::CreateFile(
		m_outputFileName, GENERIC_WRITE, 0, NULL,
		CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if(m_hFile == INVALID_HANDLE_VALUE)
	{
		m_succeeded = false;

		m_failureReason.Format(
			_T("Unable to open the file '%s' for writing."),
			m_outputFileName);
	}
	else
	{
		m_succeeded = true;

		switch(m_outputFileEncoding)
		{
		case eEncoding_UCS2:
			{
				// Write out UCS-2 byte order mark (BOM)
				// 0xFEFF
				BYTE bom[2] = {0xFF, 0xFE};
				DWORD cbWritten = 0;
				::WriteFile(m_hFile, bom, 2, &cbWritten, NULL);
			}
			break;
		case eEncoding_UTF8:
			{
				// Write out UTF-8 byte order mark (BOM)
				BYTE bom[3] = {0xEF, 0xBB, 0xBF};
				DWORD cbWritten = 0;
				::WriteFile(m_hFile, bom, 3, &cbWritten, NULL);
			}
			break;
		}
	}

	return m_succeeded;
}

void CTestWizardFindFile_SaveToFile::OnEndFindFiles(void)
{
	if(m_hFile != INVALID_HANDLE_VALUE)
	{
		::CloseHandle(m_hFile);
		m_hFile = INVALID_HANDLE_VALUE;
	}
}

void CTestWizardFindFile_SaveToFile::OnFileFound(LPCTSTR directory, LPWIN32_FIND_DATA findFileData)
{
	if(m_hFile != INVALID_HANDLE_VALUE)
	{
		LPCTSTR fileSpec = findFileData->cFileName;
		DWORD cbWritten = 0;

		switch(m_outputFileEncoding)
		{
		case eEncoding_ASCII:
			{
				USES_CONVERSION;
				LPCSTR directoryA = T2CA(directory);
				LPCSTR fileSpecA = T2CA(fileSpec);

				::WriteFile(m_hFile, directoryA, ::lstrlenA(directoryA)*sizeof(CHAR), &cbWritten, NULL);
				::WriteFile(m_hFile, "\\", 1*sizeof(CHAR), &cbWritten, NULL);
				::WriteFile(m_hFile, fileSpecA, ::lstrlenA(fileSpecA)*sizeof(CHAR), &cbWritten, NULL);
				::WriteFile(m_hFile, "\r\n", 2*sizeof(CHAR), &cbWritten, NULL);
			}
			break;
		case eEncoding_UCS2:
			{
				USES_CONVERSION;
				LPCWSTR directoryW = T2CW(directory);
				LPCWSTR fileSpecW = T2CW(fileSpec);

				::WriteFile(m_hFile, directoryW, ::lstrlenW(directoryW)*sizeof(WCHAR), &cbWritten, NULL);
				::WriteFile(m_hFile, "\\", 1*sizeof(WCHAR), &cbWritten, NULL);
				::WriteFile(m_hFile, fileSpecW, ::lstrlenW(fileSpecW)*sizeof(WCHAR), &cbWritten, NULL);
				::WriteFile(m_hFile, L"\r\n", 2*sizeof(WCHAR), &cbWritten, NULL);
			}
			break;
		case eEncoding_UTF8:
			{
				// NOTE: To preserve unicode characters, use the unicode build.
				//  Otherwise we've already "lost precision" by now with the ANSI/ASCII build
				//  and FindFirstFile(A)/FindNextFile(A).

				USES_CONVERSION;
				LPCWSTR directoryW = T2CW(directory);
				LPCWSTR fileSpecW = T2CW(fileSpec);

				// Directory
				int cchDirectoryW = ::lstrlenW(directoryW);
				int cbDirectoryMB = ::WideCharToMultiByte(CP_UTF8, 0, directoryW, cchDirectoryW, NULL, 0, NULL, NULL );
				LPSTR directoryMB = new CHAR[cbDirectoryMB];
				if(directoryMB)
				{
					::WideCharToMultiByte(CP_UTF8, 0, directoryW, cchDirectoryW, directoryMB, cbDirectoryMB, NULL, NULL );
					::WriteFile(m_hFile, directoryMB, cbDirectoryMB, &cbWritten, NULL );
					delete [] directoryMB;
					directoryMB = NULL;
				}

				// Separator
				::WriteFile(m_hFile, "\\", 1*sizeof(CHAR), &cbWritten, NULL);

				// FileSpec
				int cchFileSpecW = ::lstrlenW(fileSpecW);
				int cbFileSpecMB = ::WideCharToMultiByte(CP_UTF8, 0, fileSpecW, cchFileSpecW, NULL, 0, NULL, NULL );
				LPSTR fileSpecMB = new CHAR[cbFileSpecMB];
				if(fileSpecMB)
				{
					::WideCharToMultiByte(CP_UTF8, 0, fileSpecW, cchFileSpecW, fileSpecMB, cbFileSpecMB, NULL, NULL );
					::WriteFile(m_hFile, fileSpecMB, cbFileSpecMB, &cbWritten, NULL );
					delete [] fileSpecMB;
					fileSpecMB = NULL;
				}

				// Newline
				::WriteFile(m_hFile, "\r\n", 2*sizeof(CHAR), &cbWritten, NULL);
			}
			break;
		}
	}
}

