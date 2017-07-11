class CEditView : 
#ifndef _WIN32_WCE
	public CWindowImpl<CEditView, CRichEditCtrl>,
	public CRichEditCommands<CEditView>,
	public CRichEditFindReplaceImpl<CEditView, CFindReplaceDialogWithMessageFilter>
#else // _WIN32_WCE
	public CWindowImpl<CEditView, CEdit>,
	public CEditCommands<CEditView>
#endif // _WIN32_WCE
{
protected:
	typedef CEditView thisClass;
#ifndef _WIN32_WCE
	typedef CRichEditCommands<CEditView> editCommandsClass;
	typedef CRichEditFindReplaceImpl<CEditView, CFindReplaceDialogWithMessageFilter> findReplaceClass;
#else
	typedef CEditCommands<CEditView> editCommandsClass;
#endif

public:
#ifndef _WIN32_WCE
	DECLARE_WND_SUPERCLASS(NULL, CRichEditCtrl::GetWndClassName())
#else // _WIN32_WCE
	DECLARE_WND_SUPERCLASS(NULL, CEdit::GetWndClassName())
#endif // _WIN32_WCE

	enum
	{
		cchTAB = 8,
		nMaxBufferLen = 4000000
	};

	CFont m_font;
	int m_nRow, m_nCol;
	TCHAR m_strFilePath[MAX_PATH];
	TCHAR m_strFileName[MAX_PATH];
	BOOL m_bWordWrap;

	CEditView() : 
		m_nRow(0), m_nCol(0), 
		m_bWordWrap(FALSE)
	{
		m_font = AtlGetDefaultGuiFont();
		m_strFilePath[0] = 0;
		m_strFileName[0] = 0;
	}

	void Sorry()
	{
		MessageBox(_T("Sorry, not yet implemented"), _T("MTPad"), MB_OK);
	}

	BOOL PreTranslateMessage(MSG* pMsg)
	{
#ifndef _WIN32_WCE
		// In non Multi-thread SDI cases, CFindReplaceDialogWithMessageFilter will add itself to the
		// global message filters list.  In our case, we'll call it directly.
		if(m_pFindReplaceDialog != NULL)
		{
			if(m_pFindReplaceDialog->PreTranslateMessage(pMsg))
				return TRUE;
		}
#endif
		return FALSE;
	}

	BEGIN_MSG_MAP(CEditView)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		MESSAGE_HANDLER(WM_KEYDOWN, OnKey)
		MESSAGE_HANDLER(WM_KEYUP, OnKey)
		MESSAGE_HANDLER(WM_LBUTTONDOWN, OnKey)

		COMMAND_ID_HANDLER(ID_EDIT_PASTE, OnEditPaste)
		CHAIN_MSG_MAP_ALT(editCommandsClass, 1)

#ifndef _WIN32_WCE
		CHAIN_MSG_MAP_ALT(findReplaceClass, 1)
		COMMAND_ID_HANDLER(ID_EDIT_WORD_WRAP, OnEditWordWrap)
		COMMAND_ID_HANDLER(ID_FORMAT_FONT, OnViewFormatFont)
#endif // _WIN32_WCE
	END_MSG_MAP()

	LRESULT OnCreate(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
	{
		LRESULT lRet = DefWindowProc(uMsg, wParam, lParam);

		LimitText(nMaxBufferLen);
		SetFont(m_font);

		CClientDC dc(m_hWnd);
		HFONT hOldFont = dc.SelectFont(m_font);
		TEXTMETRIC tm;
		dc.GetTextMetrics(&tm);
		int nLogPix = dc.GetDeviceCaps(LOGPIXELSX);
		dc.SelectFont(hOldFont);
#ifndef _WIN32_WCE
		int cxTab = ::MulDiv(tm.tmAveCharWidth * cchTAB, 1440, nLogPix);	// 1440 twips = 1 inch
		if(cxTab != -1)
		{
			PARAFORMAT pf;
			pf.cbSize = sizeof(PARAFORMAT);
			pf.dwMask = PFM_TABSTOPS;
			pf.cTabCount = MAX_TAB_STOPS;
			for(int i = 0; i < MAX_TAB_STOPS; i++)
				pf.rgxTabs[i] = (i + 1) * cxTab;
			SetParaFormat(pf);
		}
#endif // _WIN32_WCE
		dc.SelectFont(hOldFont);

		SetModify(FALSE);

		return lRet;
	}

	LRESULT OnKey(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
	{
		LRESULT lRet = DefWindowProc(uMsg, wParam, lParam);

		// calc caret position
#ifndef _WIN32_WCE
		long nStartPos, nEndPos;
#else // _WIN32_WCE
		int nStartPos, nEndPos;
#endif // _WIN32_WCE
		GetSel(nStartPos, nEndPos);
		m_nRow = LineFromChar(nEndPos);
		m_nCol = 0;
		int nChar = nEndPos - LineIndex();
		if(nChar > 0)
		{
			LPTSTR lpstrLine = (LPTSTR)_alloca(max(2, (nChar + 1) * sizeof(TCHAR)));	// min = WORD for length
			nChar = GetLine(m_nRow, lpstrLine, nChar);
			for(int i = 0; i < nChar; i++)
			{
				if(lpstrLine[i] == _T('\t'))
					m_nCol = ((m_nCol / cchTAB) + 1) * cchTAB;
				else
					m_nCol++;
			}
		}

		::SendMessage(GetParent(), WM_UPDATEROWCOL, 0, 0L);

		return lRet;
	}

	LRESULT OnEditPaste(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
#ifndef _WIN32_WCE
		PasteSpecial(CF_TEXT);
#else // _WIN32_WCE
		Paste();
#endif // _WIN32_WCE
		return 0;
	}

#ifndef _WIN32_WCE
	LRESULT OnEditWordWrap(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		m_bWordWrap = !m_bWordWrap;
		int nLine = m_bWordWrap ? 0 : 1;
		SetTargetDevice(NULL, nLine);

		return 0;
	}

	LRESULT OnViewFormatFont(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		CFontDialog dlg;
		m_font.GetLogFont(&dlg.m_lf);
		dlg.m_cf.Flags |= CF_INITTOLOGFONTSTRUCT;
		if (dlg.DoModal() == IDOK)
		{
			m_font.DeleteObject();
			m_font.CreateFontIndirect(&dlg.m_lf);
			SetFont(m_font);
		}
		return 0;
	}
#endif // _WIN32_WCE

// Overrides from CEditFindReplaceImpl
#ifndef _WIN32_WCE
	CFindReplaceDialogWithMessageFilter* CreateFindReplaceDialog(BOOL bFindOnly, // TRUE for Find, FALSE for FindReplace
			LPCTSTR lpszFindWhat,
			LPCTSTR lpszReplaceWith = NULL,
			DWORD dwFlags = FR_DOWN,
			HWND hWndParent = NULL)
	{
		// In non Multi-Threaded SDI cases, we'd pass in the message loop to CFindReplaceDialogWithMessageFilter.
		// In our case, we'll call PreTranslateMessage directly from this class.
		//CFindReplaceDialogWithMessageFilter* findReplaceDialog =
		//	new CFindReplaceDialogWithMessageFilter(_Module.GetMessageLoop());
		CFindReplaceDialogWithMessageFilter* findReplaceDialog =
			new CFindReplaceDialogWithMessageFilter(NULL);

		if(findReplaceDialog == NULL)
		{
			::MessageBeep(MB_ICONHAND);
		}
		else
		{
			HWND hWndFindReplace = findReplaceDialog->Create(bFindOnly,
				lpszFindWhat, lpszReplaceWith, dwFlags, hWndParent);
			if(hWndFindReplace == NULL)
			{
				delete findReplaceDialog;
				findReplaceDialog = NULL;
			}
			else
			{
				findReplaceDialog->SetActiveWindow();
				findReplaceDialog->ShowWindow(SW_SHOW);
			}
		}

		return findReplaceDialog;
	}

	DWORD GetFindReplaceDialogFlags(void) const
	{
		DWORD dwFlags = FR_HIDEWHOLEWORD;

		if(m_bFindDown)
			dwFlags |= FR_DOWN;
		if(m_bMatchCase)
			dwFlags |= FR_MATCHCASE;

		return dwFlags;
	}
#endif // _WIN32_WCE

	BOOL LoadFile(LPCTSTR lpstrFilePath)
	{
		_ASSERTE(lpstrFilePath != NULL);

		HANDLE hFile = ::CreateFile(lpstrFilePath, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
		if(hFile == INVALID_HANDLE_VALUE)
			return FALSE;

#ifndef _WIN32_WCE
		EDITSTREAM es;
		es.dwCookie = (DWORD)hFile;
		es.dwError = 0;
		es.pfnCallback = StreamReadCallback;
		StreamIn(SF_TEXT, es);

		::CloseHandle(hFile);

		return !(BOOL)es.dwError;
#else // _WIN32_WCE
		//TODO - figure out what to do here instead...
		::CloseHandle(hFile);

		return FALSE;
#endif // _WIN32_WCE
	}

	static DWORD CALLBACK StreamReadCallback(DWORD dwCookie, LPBYTE pbBuff, LONG cb, LONG FAR *pcb)
	{
		_ASSERTE(dwCookie != 0);
		_ASSERTE(pcb != NULL);

		return !::ReadFile((HANDLE)dwCookie, pbBuff, cb, (LPDWORD)pcb, NULL);
	}

	BOOL SaveFile(LPTSTR lpstrFilePath)
	{
		_ASSERTE(lpstrFilePath != NULL);

		HANDLE hFile = ::CreateFile(lpstrFilePath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_ARCHIVE | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
		if(hFile == INVALID_HANDLE_VALUE)
			return FALSE;

#ifndef _WIN32_WCE
		EDITSTREAM es;
		es.dwCookie = (DWORD)hFile;
		es.dwError = 0;
		es.pfnCallback = StreamWriteCallback;
		StreamOut(SF_TEXT, es);

		::CloseHandle(hFile);

		return !(BOOL)es.dwError;
#else // _WIN32_WCE
		//TODO - figure out what to do here instead...
		::CloseHandle(hFile);

		return FALSE;
#endif // _WIN32_WCE
	}

	static DWORD CALLBACK StreamWriteCallback(DWORD dwCookie, LPBYTE pbBuff, LONG cb, LONG FAR *pcb)
	{
		_ASSERTE(dwCookie != 0);
		_ASSERTE(pcb != NULL);

		return !::WriteFile((HANDLE)dwCookie, pbBuff, cb, (LPDWORD)pcb, NULL);
	}

	void Init(LPCTSTR lpstrFilePath, LPCTSTR lpstrFileName)
	{
		lstrcpy(m_strFilePath, lpstrFilePath);
		lstrcpy(m_strFileName, lpstrFileName);
		SetModify(FALSE);
	}

	BOOL QueryClose()
	{
		if(!GetModify())
			return TRUE;

		CWindow wndMain(GetParent());
		TCHAR szBuff[MAX_PATH + 30];
		wsprintf(szBuff, _T("Save changes to %s ?"), m_strFileName);
		int nRet = wndMain.MessageBox(szBuff, _T("MTPad"), MB_YESNOCANCEL | MB_ICONEXCLAMATION);

		if(nRet == IDCANCEL)
			return FALSE;
		else if(nRet == IDYES)
		{
			if(!DoFileSaveAs())
				return FALSE;
		}

		return TRUE;
	}

	BOOL DoFileSaveAs()
	{
		BOOL bRet = FALSE;

		CFileDialog dlg(FALSE, NULL, m_strFilePath, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, lpcstrFilter);
		int nRet = dlg.DoModal();

		if(nRet == IDOK)
		{
			ATLTRACE(_T("File path: %s\n"), dlg.m_ofn.lpstrFile);
			bRet = SaveFile(dlg.m_ofn.lpstrFile);
			if(bRet)
				Init(dlg.m_ofn.lpstrFile, dlg.m_ofn.lpstrFileTitle);
			else
				MessageBox(_T("Error writing file!\n"));
		}

		return bRet;
	}
};
