// UrlDlg.cpp : implementation of the CUrlDlg class
//
/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#ifdef WIN32_PLATFORM_PSPC
#include "resourceppc.h"
#else
#include "resourcesp.h"
#endif

#include "UrlDlg.h"

LRESULT CUrlDlg::CEditUrl::OnKey(UINT uMsg, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled)
{
	if (wParam == VK_RETURN)
	{
		if (uMsg == WM_KEYDOWN)
			GetParent().SendMessage(WM_COMMAND, IDOK);
	}
	else
		bHandled = FALSE;
	return 1;
}

const LPCTSTR CUrlDlg::m_sTypes[3] = {L"http://", L"file://", L"res://"};

LPCTSTR CUrlDlg::GetUrl(void)
{
	return CString(m_sTypes[m_iType]) + m_strUrl;
}

void CUrlDlg::StdCloseDialog(WORD wID)
{
	if (wID == IDOK)
	{
		m_iType = m_lbType.GetCurSel();
		DoDataExchange(TRUE);
	}
	EndDialog(wID);
}

LRESULT CUrlDlg::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	CreateMenuBar(IDM_EDIT);
	DoDataExchange();
	for (int i = 0; i < 3; i++)
		m_lbType.AddString(m_sTypes[i]);
	m_lbType.SetCurSel(m_iType = 0);
	m_Ed.SetFocus();
    return bHandled = FALSE;  
}

