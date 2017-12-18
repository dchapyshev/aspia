// shellmgr.h

#ifndef __SHELLMGR_H__
#define __SHELLMGR_H__

#pragma once

#include <shlobj.h>
#include <atlctrls.h>


class CShellItemIDList
{
public:
	LPITEMIDLIST m_pidl;

	CShellItemIDList(LPITEMIDLIST pidl = NULL) : m_pidl(pidl)
	{ }

	~CShellItemIDList()
	{
		::CoTaskMemFree(m_pidl);
	}

	void Attach(LPITEMIDLIST pidl)
	{
		::CoTaskMemFree(m_pidl);
		m_pidl = pidl;
	}

	LPITEMIDLIST Detach()
	{
		LPITEMIDLIST pidl = m_pidl;
		m_pidl = NULL;
		return pidl;
	}

	bool IsNull() const
	{
		return (m_pidl == NULL);
	}

	CShellItemIDList& operator =(LPITEMIDLIST pidl)
	{
		Attach(pidl);
		return *this;
	}

	LPITEMIDLIST* operator &()
	{
		return &m_pidl;
	}

	operator LPITEMIDLIST()
	{
		return m_pidl;
	}

	operator LPCTSTR()
	{
		return (LPCTSTR)m_pidl;
	}

	operator LPTSTR()
	{
		return (LPTSTR)m_pidl;
	}

	void CreateEmpty(UINT cbSize)
	{
		::CoTaskMemFree(m_pidl);
		m_pidl = (LPITEMIDLIST)::CoTaskMemAlloc(cbSize);
		ATLASSERT(m_pidl != NULL);
		if(m_pidl != NULL)
			memset(m_pidl, 0, cbSize);
	}
};


typedef struct _LVItemData
{
	_LVItemData() : ulAttribs(0)
	{ }
	
	CComPtr<IShellFolder> spParentFolder;
	
	CShellItemIDList lpi;
	ULONG ulAttribs;

} LVITEMDATA, *LPLVITEMDATA;

typedef struct _TVItemData
{
	_TVItemData()
	{ }
	
	CComPtr<IShellFolder> spParentFolder;
	
	CShellItemIDList lpi;
	CShellItemIDList lpifq;

} TVITEMDATA, *LPTVITEMDATA;


class CShellMgr
{
public:
	int GetIconIndex(LPITEMIDLIST lpi, UINT uFlags);

	void GetNormalAndSelectedIcons(LPITEMIDLIST lpifq, LPTVITEM lptvitem);

	LPITEMIDLIST ConcatPidls(LPCITEMIDLIST pidl1, LPCITEMIDLIST pidl2);

	BOOL GetName (LPSHELLFOLDER lpsf, LPITEMIDLIST lpi, DWORD dwFlags, LPTSTR lpFriendlyName);
	LPITEMIDLIST Next(LPCITEMIDLIST pidl);
	UINT GetSize(LPCITEMIDLIST pidl);
	LPITEMIDLIST CopyITEMID(LPITEMIDLIST lpi);

	LPITEMIDLIST GetFullyQualPidl(LPSHELLFOLDER lpsf, LPITEMIDLIST lpi);

	BOOL DoContextMenu(HWND hwnd, LPSHELLFOLDER lpsfParent, LPITEMIDLIST lpi, POINT point);
};

#endif //__SHELLMGR_H__
