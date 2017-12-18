// shellmgr.cpp

#include "stdafx.h"
#include <atlctrls.h>
#include <atlctrlx.h>

#include "shellmgr.h"
#include "mainfrm.h"


int CShellMgr::GetIconIndex(LPITEMIDLIST lpi, UINT uFlags)
{
	SHFILEINFO sfi = { 0 };
	DWORD_PTR dwRet = ::SHGetFileInfo((LPCTSTR)lpi, 0, &sfi, sizeof(SHFILEINFO), uFlags);
	return (dwRet != 0) ? sfi.iIcon : -1;
}

void CShellMgr::GetNormalAndSelectedIcons(LPITEMIDLIST lpifq, LPTVITEM lptvitem)
{
	int nRet = lptvitem->iImage = GetIconIndex(lpifq, SHGFI_PIDL | SHGFI_SYSICONINDEX | SHGFI_SMALLICON);
	ATLASSERT(nRet >= 0);
	nRet = lptvitem->iSelectedImage = GetIconIndex(lpifq, SHGFI_PIDL | SHGFI_SYSICONINDEX | SHGFI_SMALLICON | SHGFI_OPENICON);
	ATLASSERT(nRet >= 0);
}

LPITEMIDLIST CShellMgr::ConcatPidls(LPCITEMIDLIST pidl1, LPCITEMIDLIST pidl2)
{
	UINT cb1 = 0;
	if (pidl1 != NULL)   // May be NULL
		cb1 = GetSize(pidl1) - sizeof(pidl1->mkid.cb);

	UINT cb2 = GetSize(pidl2);

	LPITEMIDLIST pidlNew = (LPITEMIDLIST)::CoTaskMemAlloc(cb1 + cb2);
	if (pidlNew != NULL)
	{
		if (pidl1 != NULL)
			memcpy(pidlNew, pidl1, cb1);

		memcpy(((LPSTR)pidlNew) + cb1, pidl2, cb2);
	}

	return pidlNew;
}

BOOL CShellMgr::GetName(LPSHELLFOLDER lpsf, LPITEMIDLIST lpi, DWORD dwFlags, LPTSTR lpFriendlyName)
{
	BOOL bSuccess = TRUE;
	STRRET str = { STRRET_CSTR };

	if (lpsf->GetDisplayNameOf(lpi, dwFlags, &str) == NOERROR)
	{
		USES_CONVERSION;

		switch (str.uType)
		{
		case STRRET_WSTR:
			lstrcpy(lpFriendlyName, W2CT(str.pOleStr));
			::CoTaskMemFree(str.pOleStr);
			break;
		case STRRET_OFFSET:
			lstrcpy(lpFriendlyName, (LPTSTR)lpi + str.uOffset);
			break;
		case STRRET_CSTR:
			lstrcpy(lpFriendlyName, A2CT(str.cStr));
			break;
		default:
			bSuccess = FALSE;
			break;
		}
	}
	else
	{
		bSuccess = FALSE;
	}

	return bSuccess;
}

LPITEMIDLIST CShellMgr::Next(LPCITEMIDLIST pidl)
{
	LPSTR lpMem = (LPSTR)pidl;
	lpMem += pidl->mkid.cb;
	return (LPITEMIDLIST)lpMem;
}

UINT CShellMgr::GetSize(LPCITEMIDLIST pidl)
{
	UINT cbTotal = 0;
	if (pidl != NULL)
	{
		cbTotal += sizeof(pidl->mkid.cb);   // Null terminator
		while (pidl->mkid.cb != NULL)
		{
			cbTotal += pidl->mkid.cb;
			pidl = Next(pidl);
		}
	}

	return cbTotal;
}

LPITEMIDLIST CShellMgr::CopyITEMID(LPITEMIDLIST lpi)
{
	LPITEMIDLIST lpiTemp = (LPITEMIDLIST)::CoTaskMemAlloc(lpi->mkid.cb + sizeof(lpi->mkid.cb));
	::CopyMemory((PVOID)lpiTemp, (CONST VOID*)lpi, lpi->mkid.cb + sizeof(lpi->mkid.cb));
	return lpiTemp;
}

LPITEMIDLIST CShellMgr::GetFullyQualPidl(LPSHELLFOLDER lpsf, LPITEMIDLIST lpi)
{
	TCHAR szBuff[MAX_PATH] = { 0 };

	if (!GetName(lpsf, lpi, SHGDN_FORPARSING, szBuff))
		return NULL;

	CComPtr<IShellFolder> spDeskTop;
	HRESULT hr = ::SHGetDesktopFolder(&spDeskTop);
	if (FAILED(hr))
		return NULL;

	ULONG ulEaten = 0;
	LPITEMIDLIST lpifq = NULL;
	ULONG ulAttribs = 0;
	USES_CONVERSION;
	hr = spDeskTop->ParseDisplayName(NULL, NULL, T2W(szBuff), &ulEaten, &lpifq, &ulAttribs);

	if (FAILED(hr))
		return NULL;

	return lpifq;
}

BOOL CShellMgr::DoContextMenu(HWND hWnd, LPSHELLFOLDER lpsfParent, LPITEMIDLIST lpi, POINT point)
{
	CComPtr<IContextMenu> spContextMenu;
	HRESULT hr = lpsfParent->GetUIObjectOf(hWnd, 1, (const struct _ITEMIDLIST**)&lpi, IID_IContextMenu, 0, (LPVOID*)&spContextMenu);
	if(FAILED(hr))
		return FALSE;

	HMENU hMenu = ::CreatePopupMenu();
	if(hMenu == NULL)
		return FALSE;

	// Get the context menu for the item.
	hr = spContextMenu->QueryContextMenu(hMenu, 0, 1, 0x7FFF, CMF_EXPLORE);
	if(FAILED(hr))
		return FALSE;

	int idCmd = ::TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_RETURNCMD | TPM_RIGHTBUTTON, point.x, point.y, 0, hWnd, NULL);

	if (idCmd != 0)
	{
		USES_CONVERSION;

		// Execute the command that was selected.
		CMINVOKECOMMANDINFO cmi = { 0 };
		cmi.cbSize = sizeof(CMINVOKECOMMANDINFO);
		cmi.fMask = 0;
		cmi.hwnd = hWnd;
		cmi.lpVerb = T2CA(MAKEINTRESOURCE(idCmd - 1));
		cmi.lpParameters = NULL;
		cmi.lpDirectory = NULL;
		cmi.nShow = SW_SHOWNORMAL;
		cmi.dwHotKey = 0;
		cmi.hIcon = NULL;
		hr = spContextMenu->InvokeCommand(&cmi);
	}

	::DestroyMenu(hMenu);

	return TRUE;
}
