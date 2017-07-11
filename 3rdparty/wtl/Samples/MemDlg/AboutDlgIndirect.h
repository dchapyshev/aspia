// AboutDlgIndirect.h : interface of the CAboutDlgIndirect class
//
/////////////////////////////////////////////////////////////////////////////

#pragma once

class CAboutDlgIndirect : public CIndirectDialogImpl<CAboutDlgIndirect>
{
public:
	BEGIN_DIALOG(0, 0, 187, 102)
		DIALOG_CAPTION(_T("About"))
		DIALOG_STYLE(DS_MODALFRAME | WS_POPUP | WS_CAPTION | WS_SYSMENU)
		DIALOG_FONT(8, _T("MS Shell Dlg 2"))
	END_DIALOG()

	BEGIN_CONTROLS_MAP()
		CONTROL_DEFPUSHBUTTON(_T("OK"), IDOK, 130, 81, 50, 14, 0, 0)
		CONTROL_CTEXT(_T("MemDlg Application v1.0\n\n(c) Copyright 2015"), IDC_STATIC, 25, 57, 78, 32, 0, 0)
		CONTROL_ICON(MAKEINTRESOURCE(IDR_MAINFRAME), IDC_STATIC, 55, 26, 18, 20, 0, 0)
		CONTROL_GROUPBOX(_T(""), IDC_STATIC, 7, 7, 115, 88, 0, 0)
	END_CONTROLS_MAP()

	BEGIN_MSG_MAP(CAboutDlgIndirect)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_ID_HANDLER(IDOK, OnCloseCmd)
		COMMAND_ID_HANDLER(IDCANCEL, OnCloseCmd)
	END_MSG_MAP()

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	{
		CenterWindow(GetParent());
		return TRUE;
	}

	LRESULT OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		EndDialog(wID);
		return 0;
	}
};
