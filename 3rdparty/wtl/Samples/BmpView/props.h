// props.h : interface for the properties classes
//
/////////////////////////////////////////////////////////////////////////////

#ifndef __PROPS_H__
#define __PROPS_H__

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

// not defined in original the VC6 headers
#ifndef BI_JPEG
#define BI_JPEG       4L
#endif
#ifndef BI_PNG
#define BI_PNG        5L
#endif

class CFileName : public CWindowImpl<CFileName>
{
public:
	DECLARE_WND_CLASS_EX(NULL, 0, COLOR_3DFACE)

	LPCTSTR m_lpstrFilePath;

#ifndef _WIN32_WCE
	enum { m_nToolTipID = 1313 };
	CToolTipCtrl m_tooltip;
#endif // _WIN32_WCE


	CFileName() : m_lpstrFilePath(NULL)
	{ }

	void Init(HWND hWnd, LPCTSTR lpstrName)
	{
		ATLASSERT(::IsWindow(hWnd));
		SubclassWindow(hWnd);

#ifndef _WIN32_WCE
		// Set tooltip
		m_tooltip.Create(m_hWnd);
		ATLASSERT(m_tooltip.IsWindow());
		RECT rect;
		GetClientRect(&rect);
		CToolInfo ti(0, m_hWnd, m_nToolTipID, &rect, NULL);
		m_tooltip.AddTool(&ti);

		// set text
		m_lpstrFilePath = lpstrName;
		if(m_lpstrFilePath == NULL)
			return;

		CClientDC dc(m_hWnd);	// will not really paint
		HFONT hFontOld = dc.SelectFont(GetFont());

		RECT rcText = rect;
		dc.DrawText(m_lpstrFilePath, -1, &rcText, DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_NOPREFIX | DT_CALCRECT);
		BOOL bTooLong = (rcText.right > rect.right);
		if(bTooLong)
			m_tooltip.UpdateTipText(m_lpstrFilePath, m_hWnd, m_nToolTipID);
		m_tooltip.Activate(bTooLong);

		dc.SelectFont(hFontOld);
#endif // _WIN32_WCE

		Invalidate();
		UpdateWindow();
	}

	BEGIN_MSG_MAP(CFileName)
#ifndef _WIN32_WCE
		MESSAGE_RANGE_HANDLER(WM_MOUSEFIRST, WM_MOUSELAST, OnMouseMessage)
#endif // _WIN32_WCE
		MESSAGE_HANDLER(WM_PAINT, OnPaint)
	END_MSG_MAP()

#ifndef _WIN32_WCE
	LRESULT OnMouseMessage(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
	{
		if(m_tooltip.IsWindow())
		{
			MSG msg = { m_hWnd, uMsg, wParam, lParam };
			m_tooltip.RelayEvent(&msg);
		}
		bHandled = FALSE;
		return 1;
	}
#endif // _WIN32_WCE

	LRESULT OnPaint(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	{
		CPaintDC dc(m_hWnd);
		if(m_lpstrFilePath != NULL)
		{
			RECT rect;
			GetClientRect(&rect);

			dc.SetTextColor(::GetSysColor(COLOR_WINDOWTEXT));
			dc.SetBkMode(TRANSPARENT);
			HFONT hFontOld = dc.SelectFont(GetFont());

#ifndef _WIN32_WCE
			dc.DrawText(m_lpstrFilePath, -1, &rect, DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_NOPREFIX | DT_PATH_ELLIPSIS);
#else // _WIN32_WCE
			dc.DrawText(m_lpstrFilePath, -1, &rect, DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_NOPREFIX);
#endif // _WIN32_WCE

			dc.SelectFont(hFontOld);
		}
		return 0;
	}
};

class CPageOne : public CPropertyPageImpl<CPageOne>
{
public:
	enum { IDD = IDD_PROP_PAGE1 };

	LPCTSTR m_lpstrFilePath;
	CFileName m_filename;


	CPageOne() : m_lpstrFilePath(NULL)
	{ }

	BEGIN_MSG_MAP(CPageOne)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		CHAIN_MSG_MAP(CPropertyPageImpl<CPageOne>)
	END_MSG_MAP()

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	{
		if(m_lpstrFilePath != NULL)	// get and set file properties
		{
			m_filename.Init(GetDlgItem(IDC_FILELOCATION), m_lpstrFilePath);

			WIN32_FIND_DATA fd;
			HANDLE hFind = ::FindFirstFile(m_lpstrFilePath, &fd);
			if(hFind != INVALID_HANDLE_VALUE)
			{
				int nSizeK = (int)(fd.nFileSizeLow / 1024);	// assume it not bigger than 2GB
				if(nSizeK == 0 && fd.nFileSizeLow != 0)
					nSizeK = 1;
				TCHAR szBuff[100];
				wsprintf(szBuff, _T("%i KB"), nSizeK);
				SetDlgItemText(IDC_FILESIZE, szBuff);
				SYSTEMTIME st;
				::FileTimeToSystemTime(&fd.ftCreationTime, &st);
				::GetDateFormat(LOCALE_USER_DEFAULT, 0, &st, _T("dddd, MMMM dd',' yyyy',' "), szBuff, sizeof(szBuff) / sizeof(szBuff[0]));
				TCHAR szBuff1[50];
				::GetTimeFormat(LOCALE_USER_DEFAULT, 0, &st, _T("hh':'mm':'ss tt"), szBuff1, sizeof(szBuff1) / sizeof(szBuff1[0]));
				lstrcat(szBuff, szBuff1);
				SetDlgItemText(IDC_FILEDATE, szBuff);

				szBuff[0] = 0;
				if((fd.dwFileAttributes & FILE_ATTRIBUTE_ARCHIVE) != 0)
					lstrcat(szBuff, _T("Archive, "));
				if((fd.dwFileAttributes & FILE_ATTRIBUTE_READONLY) != 0)
					lstrcat(szBuff, _T("Read-only, "));
				if((fd.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) != 0)
					lstrcat(szBuff, _T("Hidden, "));
				if((fd.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM) != 0)
					lstrcat(szBuff, _T("System"));
				int nLen = lstrlen(szBuff);
				if(nLen >= 2 && szBuff[nLen - 2] == _T(','))
					szBuff[nLen - 2] = 0;
				SetDlgItemText(IDC_FILEATTRIB, szBuff);

				::FindClose(hFind);
			}
		}
		else
		{
			SetDlgItemText(IDC_FILELOCATION, _T("(Clipboard)"));
			SetDlgItemText(IDC_FILESIZE, _T("N/A"));
			SetDlgItemText(IDC_FILEDATE, _T("N/A"));
			SetDlgItemText(IDC_FILEATTRIB, _T("N/A"));
		}

		return TRUE;
	}
};

class CPageTwo : public CPropertyPageImpl<CPageTwo>
{
public:
	enum { IDD = IDD_PROP_PAGE2 };

	LPCTSTR m_lpstrFilePath;
	CBitmapHandle m_bmp;


	CPageTwo() : m_lpstrFilePath(NULL)
	{ }

	BEGIN_MSG_MAP(CPageTwo)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		CHAIN_MSG_MAP(CPropertyPageImpl<CPageTwo>)
	END_MSG_MAP()

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	{
		// Special - remove unused buttons, move Close button, center wizard
		CPropertySheetWindow sheet = GetPropertySheet();
#if !defined(_AYGSHELL_H_) && !defined(__AYGSHELL_H__) // PPC specific
		sheet.CancelToClose();
		RECT rect;
		CButton btnCancel = sheet.GetDlgItem(IDCANCEL);
		btnCancel.GetWindowRect(&rect);
		sheet.ScreenToClient(&rect);
		btnCancel.ShowWindow(SW_HIDE);
		CButton btnClose = sheet.GetDlgItem(IDOK);
		btnClose.SetWindowPos(NULL, &rect, SWP_NOZORDER | SWP_NOSIZE);
		sheet.CenterWindow(GetPropertySheet().GetParent());

		sheet.ModifyStyleEx(WS_EX_CONTEXTHELP, 0);
#endif // (_AYGSHELL_H_) || defined(__AYGSHELL_H__) // PPC specific

		// get and display bitmap prperties
		SetDlgItemText(IDC_TYPE, _T("Windows 3.x Bitmap (BMP)"));
		LPTSTR lpstrCompression = _T("Uncompressed");;

		if(m_lpstrFilePath != NULL)
		{
#ifndef _WIN32_WCE 
			HANDLE hFile = ::CreateFile(m_lpstrFilePath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
#else
			HANDLE hFile = ::CreateFile(m_lpstrFilePath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
#endif // _WIN32_WCE
			ATLASSERT(hFile != INVALID_HANDLE_VALUE);
			if(hFile != INVALID_HANDLE_VALUE)
			{
				DWORD dwRead = 0;
				BITMAPFILEHEADER bfh;
				::ReadFile(hFile, &bfh, sizeof(bfh), &dwRead, NULL);
				BITMAPINFOHEADER bih;
				::ReadFile(hFile, &bih, sizeof(bih), &dwRead, NULL);
				::CloseHandle(hFile);

				SetDlgItemInt(IDC_WIDTH, bih.biWidth);
				SetDlgItemInt(IDC_HEIGHT, bih.biHeight);
				SetDlgItemInt(IDC_HORRES, ::MulDiv(bih.biXPelsPerMeter, 254, 10000));
				SetDlgItemInt(IDC_VERTRES, ::MulDiv(bih.biYPelsPerMeter, 254, 10000));
				SetDlgItemInt(IDC_BITDEPTH, bih.biBitCount);

				switch(bih.biCompression)
				{
#ifndef _WIN32_WCE
				case BI_RLE4:
				case BI_RLE8:
					lpstrCompression = _T("RLE");
					break;
#endif // _WIN32_WCE
				case BI_BITFIELDS:
					lpstrCompression = _T("Uncompressed with bitfields");
					break;
				case BI_JPEG:
				case BI_PNG:
					lpstrCompression = _T("Unknown");
					break;
				}
				SetDlgItemText(IDC_COMPRESSION, lpstrCompression);
			}
		}
		else		// must be pasted from the clipboard
		{
			ATLASSERT(!m_bmp.IsNull());
			BITMAP bitmap = { 0 };
			bool bRet = m_bmp.GetBitmap(bitmap);
			ATLASSERT(bRet);
			if(bRet)
			{
				CClientDC dc(NULL);
				SetDlgItemInt(IDC_WIDTH, bitmap.bmWidth);
				SetDlgItemInt(IDC_HEIGHT, bitmap.bmHeight);
				// should we use screen resolution here???
				SetDlgItemInt(IDC_HORRES, dc.GetDeviceCaps(LOGPIXELSX));
				SetDlgItemInt(IDC_VERTRES, dc.GetDeviceCaps(LOGPIXELSX));
				SetDlgItemInt(IDC_BITDEPTH, bitmap.bmBitsPixel);
				SetDlgItemText(IDC_COMPRESSION, lpstrCompression);
			}
		}

		return TRUE;
	}
};

class CPageThree : public CPropertyPageImpl<CPageThree>
{
public:
	enum { IDD = IDD_PROP_PAGE3 };

	BEGIN_MSG_MAP(CPageThree)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		CHAIN_MSG_MAP(CPropertyPageImpl<CPageThree>)
	END_MSG_MAP()

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	{
		// get and set screen properties
		CClientDC dc(NULL);
		SetDlgItemInt(IDC_WIDTH, dc.GetDeviceCaps(HORZRES));
		SetDlgItemInt(IDC_HEIGHT, dc.GetDeviceCaps(VERTRES));
		SetDlgItemInt(IDC_HORRES, dc.GetDeviceCaps(LOGPIXELSX));
		SetDlgItemInt(IDC_VERTRES, dc.GetDeviceCaps(LOGPIXELSY));
		SetDlgItemInt(IDC_BITDEPTH, dc.GetDeviceCaps(BITSPIXEL));

		return TRUE;
	}
};

class CBmpProperties : public CPropertySheetImpl<CBmpProperties>
{
public:
	CPageOne m_page1;
	CPageTwo m_page2;
	CPageThree m_page3;


	CBmpProperties()
	{
		m_psh.dwFlags |= PSH_NOAPPLYNOW;

		AddPage(m_page1);
		AddPage(m_page2);
		AddPage(m_page3);
		SetActivePage(1);
		SetTitle(_T("Bitmap Properties"));
	}

	void SetFileInfo(LPCTSTR lpstrFilePath, HBITMAP hBitmap)
	{
		m_page1.m_lpstrFilePath = lpstrFilePath;
		m_page2.m_lpstrFilePath = lpstrFilePath;
		m_page2.m_bmp = hBitmap;
	}

	BEGIN_MSG_MAP(CBmpProperties)
		CHAIN_MSG_MAP(CPropertySheetImpl<CBmpProperties>)
	END_MSG_MAP()
};

#endif // __PROPS_H__
