class CEditView : 
	public CWindowImpl<CEditView, CRichEditCtrl>,
	public CRichEditCommands<CEditView>,
	public CRichEditFindReplaceImpl<CEditView, CFindReplaceDialogWithMessageFilter>
{
protected:
	typedef CEditView thisClass;
	typedef CRichEditCommands<CEditView> editCommandsClass;
	typedef CRichEditFindReplaceImpl<CEditView, CFindReplaceDialogWithMessageFilter> findReplaceClass;

public:
	DECLARE_WND_SUPERCLASS(NULL, CRichEditCtrl::GetWndClassName())

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
		// Set "Segoe UI" font for Vista or higher, or "Tahoma" otherwise
		CLogFont lf;
		lf.lfWeight = FW_NORMAL;
		if(RunTimeHelper::IsVista())
		{
			lf.SetHeight(9);
			SecureHelper::strcpy_x(lf.lfFaceName, LF_FACESIZE, _T("Segoe UI"));
		}
		else
		{
			lf.SetHeight(8);
			SecureHelper::strcpy_x(lf.lfFaceName, LF_FACESIZE, _T("Tahoma"));
		}
		m_font.CreateFontIndirect(&lf);

		m_strFilePath[0] = 0;
		m_strFileName[0] = 0;
	}

	void Sorry()
	{
		MessageBox(_T("Sorry, not yet implemented"), _T("MTPad"), MB_OK);
	}

	BOOL PreTranslateMessage(MSG* pMsg)
	{
		// In non Multi-thread SDI cases, CFindReplaceDialogWithMessageFilter will add itself to the
		// global message filters list.  In our case, we'll call it directly.
		if(m_pFindReplaceDialog != NULL)
		{
			if(m_pFindReplaceDialog->PreTranslateMessage(pMsg))
				return TRUE;
		}
		return FALSE;
	}

	BEGIN_MSG_MAP(CEditView)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		MESSAGE_HANDLER(WM_KEYDOWN, OnKey)
		MESSAGE_HANDLER(WM_KEYUP, OnKey)
		MESSAGE_HANDLER(WM_LBUTTONDOWN, OnKey)

		COMMAND_ID_HANDLER(ID_EDIT_PASTE, OnEditPaste)
		CHAIN_MSG_MAP_ALT(editCommandsClass, 1)

		CHAIN_MSG_MAP_ALT(findReplaceClass, 1)
		COMMAND_ID_HANDLER(ID_EDIT_WORD_WRAP, OnEditWordWrap)
		RIBBON_FONT_CONTROL_HANDLER(ID_RIBBON_FONT, OnRibbonFont)
		COMMAND_ID_HANDLER(ID_FORMAT_FONT, OnViewFormatFont)
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

		dc.SelectFont(hOldFont);
		SetModify(FALSE);

		return lRet;
	}

	LRESULT OnKey(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
	{
		LRESULT lRet = DefWindowProc(uMsg, wParam, lParam);

		// calc caret position
		long nStartPos, nEndPos;
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
		PasteSpecial(CF_TEXT);
		return 0;
	}

	LRESULT OnEditWordWrap(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		m_bWordWrap = !m_bWordWrap;
		int nLine = m_bWordWrap ? 0 : 1;
		SetTargetDevice(NULL, nLine);

		return 0;
	}

	LRESULT OnRibbonFont(UI_EXECUTIONVERB verb, WORD /*wID*/, CHARFORMAT2* pcf, BOOL& /*bHandled*/)
	{
		if (verb == UI_EXECUTIONVERB_CANCELPREVIEW)
			SetFont(m_font);
		else
			SetDefaultCharFormat(*pcf);

		if (verb == UI_EXECUTIONVERB_EXECUTE)
			UpdateFont(*pcf);

		return 0;
	}

	void UpdateFont(CHARFORMAT2& cf)
	{
		CLogFont lf(m_font);

		if (cf.dwMask & CFM_SIZE)
		{
			lf.lfHeight = -MulDiv(cf.yHeight, 
				CWindowDC(NULL).GetDeviceCaps(LOGPIXELSY), 1440);
			lf.lfWidth = 0;
		}
		
		if (cf.dwMask & CFM_BOLD)
			lf.lfWeight = cf.dwEffects & CFE_BOLD ? FW_BOLD : 0;

		if (cf.dwMask & CFM_WEIGHT)
			lf.lfWeight = cf.wWeight;
		
		if (cf.dwMask & CFM_ITALIC)
			lf.lfItalic = cf.dwEffects & CFE_ITALIC ? TRUE : FALSE;
		
		if (cf.dwMask & CFM_UNDERLINE)
			lf.lfUnderline = cf.dwEffects & CFE_UNDERLINE ? TRUE : FALSE;
		
		if (cf.dwMask & CFM_STRIKEOUT)
			lf.lfStrikeOut = cf.dwEffects & CFE_STRIKEOUT ? TRUE : FALSE;
		
		if (cf.dwMask & CFM_CHARSET)
			lf.lfCharSet = cf.bCharSet;
		
		if (cf.dwMask & CFM_FACE)
		{
			lf.lfPitchAndFamily = cf.bPitchAndFamily;
			SecureHelper::strcpyW_x(lf.lfFaceName, LF_FACESIZE, cf.szFaceName);
		}

		m_font.DeleteObject();
		m_font.CreateFontIndirect(&lf);
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

// Overrides from CEditFindReplaceImpl
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

	BOOL LoadFile(LPCTSTR lpstrFilePath)
	{
		_ASSERTE(lpstrFilePath != NULL);

		HANDLE hFile = ::CreateFile(lpstrFilePath, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
		if(hFile == INVALID_HANDLE_VALUE)
			return FALSE;

		EDITSTREAM es;
		es.dwCookie = (DWORD)hFile;
		es.dwError = 0;
		es.pfnCallback = StreamReadCallback;
		StreamIn(SF_TEXT, es);

		::CloseHandle(hFile);

		return !(BOOL)es.dwError;
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

		EDITSTREAM es;
		es.dwCookie = (DWORD)hFile;
		es.dwError = 0;
		es.pfnCallback = StreamWriteCallback;
		StreamOut(SF_TEXT, es);

		::CloseHandle(hFile);

		return !(BOOL)es.dwError;
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
