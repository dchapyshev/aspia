class CMTPadMsgLoop : public CMessageLoop
{
public:
	CMainFrame m_wndFrame;

	int Run(LPTSTR lpstrCmdLine, int nCmdShow)
	{
		if(m_wndFrame.CreateEx() == NULL)
		{
			ATLTRACE(_T("CMainFrame creation failed!\n"));
			return 0;
		}

		if(lpstrCmdLine[0] == 0)
		{
			m_wndFrame.DoFileNew();
		}
		else	// file name specified at the command line
		{
			// strip quotes (if any)
			LPTSTR lpstrCmd = lpstrCmdLine;
			if(lpstrCmd[0] == '"')
			{
				lpstrCmd++;
				for(int i = 0; i < lstrlen(lpstrCmd); i++)
				{
					if(lpstrCmd[i] == '"')
					{
						lpstrCmd[i] = 0;
						break;
					}
				}
			}
			// get full path and file name
			TCHAR szPathName[MAX_PATH];
			LPTSTR lpstrFileName = NULL;
#ifndef _WIN32_WCE
			::GetFullPathName(lpstrCmd, MAX_PATH, szPathName, &lpstrFileName);
#endif // _WIN32_WCE
			// open file
			if(m_wndFrame.DoFileOpen(szPathName, lpstrFileName))
			{
#ifndef _WIN32_WCE
				m_wndFrame.m_mru.AddToList(szPathName);
#endif // _WIN32_WCE
			}
		}

		m_wndFrame.ShowWindow(nCmdShow);

		return CMessageLoop::Run();
	}

	virtual BOOL PreTranslateMessage(MSG* pMsg)
	{
		return m_wndFrame.PreTranslateMessage(pMsg);
	}

	virtual BOOL OnIdle(int)
	{
		m_wndFrame.UpdateUIAll();
		return FALSE;
	}
};

class CThreadManager
{
public:
	// thread init param
	struct _RunData
	{
		LPTSTR lpstrCmdLine;
		int nCmdShow;
	};

	// thread proc
	static DWORD WINAPI RunThread(LPVOID lpData)
	{
		CMTPadMsgLoop theLoop;
		_Module.AddMessageLoop(&theLoop);

		_RunData* pData = (_RunData*)lpData;
		int nRet = theLoop.Run(pData->lpstrCmdLine, pData->nCmdShow);
		delete pData;

		_Module.RemoveMessageLoop();
		return nRet;
	}

	DWORD m_dwCount;
	HANDLE m_arrThreadHandles[MAXIMUM_WAIT_OBJECTS - 1];

	CThreadManager() : m_dwCount(0)
	{ }

// Operations
	DWORD AddThread(LPTSTR lpstrCmdLine, int nCmdShow)
	{
		if(m_dwCount == (MAXIMUM_WAIT_OBJECTS - 1))
		{
			::MessageBox(NULL, _T("ERROR: Cannot create ANY MORE threads!!!"), _T("MTPad"), MB_OK);
			return 0;
		}

		_RunData* pData = new _RunData;
		pData->lpstrCmdLine = lpstrCmdLine;
		pData->nCmdShow = nCmdShow;
		DWORD dwThreadID;
		HANDLE hThread = ::CreateThread(NULL, 0, RunThread, pData, 0, &dwThreadID);
		if(hThread == NULL)
		{
			::MessageBox(NULL, _T("Cannot create thread!!!"), _T("MTPad"), MB_OK);
			return 0;
		}

		m_arrThreadHandles[m_dwCount] = hThread;
		m_dwCount++;
		return dwThreadID;
	}

	void RemoveThread(DWORD dwIndex)
	{
		::CloseHandle(m_arrThreadHandles[dwIndex]);
		if(dwIndex != (m_dwCount - 1))
			m_arrThreadHandles[dwIndex] = m_arrThreadHandles[m_dwCount - 1];
		m_dwCount--;
	}

	int Run(LPTSTR lpstrCmdLine, int nCmdShow)
	{
		MSG msg;
		// force message queue to be created
		::PeekMessage(&msg, NULL, WM_USER, WM_USER, PM_NOREMOVE);

		AddThread(lpstrCmdLine, nCmdShow);

		int nRet = m_dwCount;
		DWORD dwRet;
		while(m_dwCount > 0)
		{
			dwRet = ::MsgWaitForMultipleObjects(m_dwCount, m_arrThreadHandles, FALSE, INFINITE, QS_ALLINPUT);

			if(dwRet == 0xFFFFFFFF)
				::MessageBox(NULL, _T("Wait for multiple objects failed!!!"), _T("MTPad"), MB_OK);
			else if(dwRet >= WAIT_OBJECT_0 && dwRet <= (WAIT_OBJECT_0 + m_dwCount - 1))
				RemoveThread(dwRet - WAIT_OBJECT_0);
			else if(dwRet == (WAIT_OBJECT_0 + m_dwCount))
			{
				::GetMessage(&msg, NULL, 0, 0);
				if(msg.message == WM_USER)
					AddThread(_T(""), SW_SHOWNORMAL);
				else
					::MessageBeep((UINT)-1);
			}
			else
				::MessageBeep((UINT)-1);
		}

		return nRet;
	}
};
