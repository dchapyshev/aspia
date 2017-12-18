#ifndef __FolderDialogStatusText_h__
#define __FolderDialogStatusText_h__

#pragma once

#ifndef __cplusplus
	#error ATL requires C++ compilation (use a .cpp suffix)
#endif

#ifndef __ATLAPP_H__
	#error FolderDialogStatusText.h requires atlapp.h to be included first
#endif

#ifndef __ATLWIN_H__
	#error FolderDialogStatusText.h requires atlwin.h to be included first
#endif

#ifndef __ATLWIN_H__
	#error FolderDialogStatusText.h requires atlwin.h to be included first
#endif

#ifndef __ATLDLGS_H__
	#error FolderDialogStatusText.h requires atldlgs.h to be included first
#endif

class CFolderDialogStatusText :
	public CFolderDialogImpl<CFolderDialogStatusText>
{
protected:
// Typedefs
	typedef CFolderDialogImpl<CFolderDialogStatusText> baseClass;

public:
// Enumerations
	enum DialogIds
	{
		// NOTE: Dialog IDs found using SPY++
		_IDC_STATUSTEXT = 0x00003741,
		_IDC_TITLE      = 0x00003742,
	};

// Constructor
	CFolderDialogStatusText(HWND hWndParent = NULL, LPCTSTR lpstrTitle = NULL, UINT uFlags = BIF_NEWDIALOGSTYLE | BIF_RETURNFSANCESTORS | BIF_RETURNONLYFSDIRS | BIF_STATUSTEXT)
		: baseClass(hWndParent, lpstrTitle, uFlags)
	{
	}

// Overrides from base class
	void OnInitialized()
	{
		// NOTE: We compensate for the issue where BIF_STATUSTEXT
		//  isn't honored if BIF_NEWDIALOGSTYLE is present.
		//  Some would say its a bug, some would say its by design...
		if((m_bi.ulFlags & BIF_NEWDIALOGSTYLE) == BIF_NEWDIALOGSTYLE &&
			(m_bi.ulFlags & BIF_STATUSTEXT) == BIF_STATUSTEXT)
		{
			int fontHeight = 11;  // 8 point Tahoma at 96 DPI

			CWindow title = ::GetDlgItem(m_hWnd, _IDC_TITLE);
			RECT rcTitle = {0};
			if(title)
			{
				CFontHandle titleFont = title.GetFont();
				CLogFont logFont;
				titleFont.GetLogFont(logFont);
				if(logFont.lfHeight < 0)
					fontHeight = -1*logFont.lfHeight;

				title.GetWindowRect(&rcTitle);
				::MapWindowPoints(NULL, title.GetParent(), (LPPOINT)&rcTitle, 2);
				rcTitle.top -= 1;
				rcTitle.bottom -= fontHeight + 1;
				title.SetWindowPos(NULL, &rcTitle, (SWP_NOACTIVATE | SWP_NOZORDER | SWP_SHOWWINDOW));
			}

			CWindow status = ::GetDlgItem(m_hWnd, _IDC_STATUSTEXT);
			if(status)
			{
				status.EnableWindow(TRUE);
				status.ModifyStyle(0,SS_PATHELLIPSIS|SS_NOPREFIX);

				rcTitle.top = rcTitle.bottom + 2;
				rcTitle.bottom = rcTitle.top + fontHeight + (fontHeight/2);

				status.SetWindowPos(NULL, &rcTitle, (SWP_NOACTIVATE | SWP_NOZORDER | SWP_SHOWWINDOW));
			}

			// NOTE: We'd really like to have the status window resize appropriately
			//  when the dialog resizes, but currently that doesn't happen.
		}
	}

	void OnSelChanged(LPITEMIDLIST pItemIDList)
	{
		if((m_bi.ulFlags & BIF_STATUSTEXT) == BIF_STATUSTEXT)
		{
			TCHAR sNewFolder[MAX_PATH+1] = {0};
			if(::SHGetPathFromIDList(pItemIDList,sNewFolder))
			{
				if((m_bi.ulFlags & BIF_NEWDIALOGSTYLE) == BIF_NEWDIALOGSTYLE)
					::SetDlgItemText(m_hWnd, _IDC_STATUSTEXT, sNewFolder);
				else
					this->SetStatusText(sNewFolder);
			}
		}
	}
};

#endif // __FolderDialogStatusText_h__
