// ImageViewdlg.h : interface of the ImageView dialog and property sheet and pages classes
//
/////////////////////////////////////////////////////////////////////////////

#if !defined(_IMAGEVIEW_DLG_H__)
#define _IMAGEVIEW_DLG_H__

#pragma once

class CMainFrame; // forward declaration

/////////////////////
// CAboutDlg 

class CAboutDlg : public CStdSimpleDialogResizeImpl<CAboutDlg, 
	IDD_ABOUTBOX, SHIDIF_DONEBUTTON | SHIDIF_FULLSCREENNOMENUBAR>
{
public:
	BEGIN_DLGRESIZE_MAP(CAboutDlg)
		BEGIN_DLGRESIZE_GROUP()
			DLGRESIZE_CONTROL(IDC_HEAD, DLSZ_MOVE_X | DLSZ_MOVE_Y)
			DLGRESIZE_CONTROL(IDC_APPICON, DLSZ_MOVE_X | DLSZ_MOVE_Y)
			DLGRESIZE_CONTROL(IDC_INFOSTATIC, DLSZ_SIZE_X | DLSZ_SIZE_Y)
		END_DLGRESIZE_GROUP()
	END_DLGRESIZE_MAP()
};

/////////////////////
// CMoveDlg :  Called by CRegisterDlg to move ImageView.exe to \Windows folder

class CMoveDlg : public  CStdDialogResizeImpl<CMoveDlg,SHIDIF_FULLSCREENNOMENUBAR>
{
public:
	CString m_sAppPath;
	CString m_sApp;

	enum { IDD = IDD_MOVE };

	typedef CStdDialogResizeImpl<CMoveDlg,SHIDIF_FULLSCREENNOMENUBAR> baseClass;

	BEGIN_DLGRESIZE_MAP(CMoveDlg)
		BEGIN_DLGRESIZE_GROUP()
			DLGRESIZE_CONTROL(IDC_FILELOCATION_H, DLSZ_SIZE_X | DLSZ_SIZE_Y)
			DLGRESIZE_CONTROL(IDC_FILELOCATION, DLSZ_SIZE_X | DLSZ_MOVE_Y)
			DLGRESIZE_CONTROL(IDC_INFOSTATIC, DLSZ_SIZE_X | DLSZ_SIZE_Y)
			DLGRESIZE_CONTROL(IDC_SHORTCUT, DLSZ_SIZE_X | DLSZ_MOVE_Y)
			DLGRESIZE_CONTROL(IDC_MOVE_T, DLSZ_MOVE_X | DLSZ_MOVE_Y)
			DLGRESIZE_CONTROL(IDC_MOVE, DLSZ_MOVE_X | DLSZ_MOVE_Y)
			DLGRESIZE_CONTROL(IDCANCEL, DLSZ_MOVE_X | DLSZ_MOVE_Y)
		END_DLGRESIZE_GROUP()
	END_DLGRESIZE_MAP()

	BEGIN_MSG_MAP(CMoveDlg)
		COMMAND_HANDLER(IDC_MOVE, BN_CLICKED, OnMove)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		CHAIN_MSG_MAP(baseClass)
	END_MSG_MAP()
		
	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
	{
		SHDoneButton( m_hWnd, SHDB_HIDE);

		GetModuleFileName( NULL, m_sAppPath.GetBuffer(MAX_PATH+1), MAX_PATH);
		m_sAppPath.ReleaseBuffer();

		SetDlgItemText( IDC_FILELOCATION, m_sAppPath);
		m_sApp = m_sAppPath.Mid( m_sAppPath.ReverseFind(L'\\') + 1);
		
		CheckDlgButton( IDC_SHORTCUT, TRUE);

		return bHandled=FALSE;
	}

// Move operation
	LRESULT OnMove(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		CString sDest = L"\\Windows\\" + m_sApp;
		if ( ::MoveFile( m_sAppPath, sDest))
		{
			if ( IsDlgButtonChecked( IDC_SHORTCUT))
			{
				m_sAppPath.Replace( L".exe", L".lnk");
				if ( !::SHCreateShortcut( (LPTSTR)(LPCTSTR)m_sAppPath, (LPTSTR)(LPCTSTR)sDest))
					AtlMessageBox( m_hWnd, L"Cannot create shortcut to ImageView.", 
						IDR_MAINFRAME, MB_OK | MB_ICONWARNING);
			}
			EndDialog(IDOK);
		}
		else
			AtlMessageBox( m_hWnd, L"Cannot move ImageView.exe to \\Windows folder.", 
				IDR_MAINFRAME, MB_OK | MB_ICONERROR);
		return 0;
	}
};

/////////////////////
// CRegisterDlg: Register ImageView.exe as standard program for image files 

#ifdef _WTL_CE_DRA
class CRegisterDlg : public  CStdOrientedDialogImpl<CRegisterDlg, SHIDIF_DONEBUTTON | SHIDIF_FULLSCREENNOMENUBAR>
#else
class CRegisterDlg : public  CStdDialogResizeImpl<CRegisterDlg, SHIDIF_DONEBUTTON | SHIDIF_FULLSCREENNOMENUBAR>
#endif
{
public:

#ifdef _WTL_CE_DRA
	typedef CStdOrientedDialogImpl<CRegisterDlg, SHIDIF_DONEBUTTON | SHIDIF_FULLSCREENNOMENUBAR> baseClass;
	enum {IDD = IDD_REGISTER, IDD_LANDSCAPE = IDD_REGISTER_L};
#else
	typedef CStdDialogResizeImpl<CRegisterDlg, SHIDIF_DONEBUTTON | SHIDIF_FULLSCREENNOMENUBAR> baseClass;
	enum {IDD = IDD_REGISTER};
#endif // _WTL_CE_DRA

	CString m_sAppPath;
	CString m_sApp;

// Image type enumeration
	enum ImageType { BMP = IDC_BMP, JPG, PNG, GIF } ;

// Implementation
// Helper class: image command registry key
	class CImageTypeKey : public CRegKeyEx
	{
	public:
		CString m_sCmd;
		DWORD size;

		CImageTypeKey( ImageType typ) : size( MAX_PATH)
		{
			CString sKey = GetTypeString( typ);
			sKey += L"\\Shell\\Open\\Command";
			Open( HKEY_CLASSES_ROOT, sKey);
		}

		LPCTSTR GetTypeString( ImageType typ)
		{
			switch ( typ)
			{
			case BMP : return L"bmpimage"; 
			case JPG : return L"jpegimage"; 
			case PNG : return L"pngimage"; 
			case GIF : return L"gifimage"; 
			default : ATLASSERT( FALSE); return NULL;
			}
		}

		LPCTSTR GetCmd()
		{
			QueryStringValue(L"",  m_sCmd.GetBuffer( size), &size);
			m_sCmd.ReleaseBuffer();
			return m_sCmd;
		}

		void SetCmd(LPCTSTR sCmd)
		{
			SetStringValue(L"", sCmd);
		}
	};

// Image type file extension
		LPCTSTR GetExtString( ImageType typ)
		{
			switch ( typ)
			{
			case BMP : return L".bmp"; ;
			case JPG : return L".jpg"; 
			case PNG : return L".png"; 
			case GIF : return L".gif"; 
			default : ATLASSERT( FALSE); return NULL;
			}
		}

// Image type registration status
	bool IsRegistered( ImageType typ)
	{
		CImageTypeKey key( typ);
		CString sCmd = key.GetCmd();
		return sCmd.Find( m_sApp) != -1 ;		
	}
	
// Image type registration-deregistration
	void Register( ImageType typ, BOOL bRegister)
	{
		CImageTypeKey key( typ);
		CString sOldCmd = key.GetCmd();
		CString sNewCmd = m_sAppPath;
		CAppInfoT<CMainFrame> info;

		if ( bRegister)
			sNewCmd += L" %1";
		else
			info.Restore( sNewCmd, key.GetTypeString( typ));
		
		key.SetCmd( sNewCmd);
		
		if ( bRegister)
			info.Save( sOldCmd, key.GetTypeString( typ));
		else 
			info.Delete( key.GetTypeString( typ));
	}
	
#ifndef _WTL_CE_DRA
	BEGIN_DLGRESIZE_MAP(CMoveDlg)
		BEGIN_DLGRESIZE_GROUP()
			DLGRESIZE_CONTROL(IDC_INFOSTATIC, DLSZ_SIZE_X | DLSZ_SIZE_Y)
			DLGRESIZE_CONTROL(IDC_REGISTER_H, DLSZ_SIZE_X | DLSZ_SIZE_Y)
			DLGRESIZE_CONTROL(IDC_IBMP, DLSZ_MOVE_X | DLSZ_MOVE_Y)
			DLGRESIZE_CONTROL(IDC_BMP, DLSZ_MOVE_X | DLSZ_MOVE_Y)
			DLGRESIZE_CONTROL(IDC_IJPG, DLSZ_MOVE_X | DLSZ_MOVE_Y)
			DLGRESIZE_CONTROL(IDC_JPG, DLSZ_MOVE_X | DLSZ_MOVE_Y)
			DLGRESIZE_CONTROL(IDC_IPNG, DLSZ_MOVE_X | DLSZ_MOVE_Y)
			DLGRESIZE_CONTROL(IDC_PNG, DLSZ_MOVE_X | DLSZ_MOVE_Y)
			DLGRESIZE_CONTROL(IDC_IGIF, DLSZ_MOVE_X | DLSZ_MOVE_Y)
			DLGRESIZE_CONTROL(IDC_GIF, DLSZ_MOVE_X | DLSZ_MOVE_Y)
		END_DLGRESIZE_GROUP()
	END_DLGRESIZE_MAP()
#else
	void OnOrientation(DRA::DisplayMode mode)
	{
// Text controls re-initialization
		for(int iBtn = IDC_BMP ; iBtn <= IDC_GIF ; iBtn++)
		{
			SHFILEINFO sfi;
			::SHGetFileInfo( GetExtString( (ImageType)iBtn), FILE_ATTRIBUTE_NORMAL, &sfi, sizeof(sfi), 
				SHGFI_USEFILEATTRIBUTES | SHGFI_TYPENAME );
			SetDlgItemText( iBtn, sfi.szTypeName);
			CheckDlgButton( iBtn, IsRegistered( (ImageType)iBtn));
		}
	}
#endif // ndef _WTL_CE_DRA

	BEGIN_MSG_MAP(CRegisterDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_RANGE_HANDLER(IDC_BMP, IDC_GIF, OnCheckType)
		CHAIN_MSG_MAP(baseClass)
	END_MSG_MAP()

// Dialog initialization
	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
	{
		::GetModuleFileName( NULL, m_sAppPath.GetBuffer( MAX_PATH + 1), MAX_PATH);
		m_sAppPath.ReleaseBuffer();
		m_sApp = m_sAppPath.Mid( m_sAppPath.ReverseFind(L'\\') + 1);

		// Call CMoveDlg if ImageView.exe is not located in \Windows folder
		if( CString(L"\\Windows\\") + m_sApp != m_sAppPath)
		{
			CMoveDlg dlg;
			if ( dlg.DoModal() == IDCANCEL)
				EndDialog( IDCANCEL);/**/

			::GetModuleFileName( NULL, m_sAppPath.GetBuffer( MAX_PATH + 1), MAX_PATH);
			m_sAppPath.ReleaseBuffer();
		}

// Controls initialization: IDC_BMP, IDC_JPG etc... MUST be in sequence.
		for( int iBtn = IDC_BMP, iIcon = IDC_IBMP ; iBtn <= IDC_GIF ; iBtn++, iIcon++)
		{
			SHFILEINFO sfi;
			::SHGetFileInfo( GetExtString( (ImageType)iBtn), FILE_ATTRIBUTE_NORMAL, &sfi, sizeof(sfi), 
				SHGFI_USEFILEATTRIBUTES | SHGFI_ICON | SHGFI_TYPENAME );
			SendDlgItemMessage( iIcon, STM_SETIMAGE, IMAGE_ICON, (LPARAM)sfi.hIcon);
			SetDlgItemText( iBtn, sfi.szTypeName);
			CheckDlgButton( iBtn, IsRegistered( (ImageType)iBtn));
		}

		return bHandled = FALSE; // to prevent CDialogImplBaseT< TBase >::DialogProc settings
	}

// Operation
	LRESULT OnCheckType(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		Register( (ImageType)wID, IsDlgButtonChecked( wID));
		return 0;
	}
};


/////////////////////
// CFilePage: Current image file propertes 

class CFilePage : public CPropertyPageImpl<CFilePage>, public CDialogResize<CFilePage>
{
public:
	enum { IDD = IDD_PROP_FILE };

	CString m_sPath;

	CFilePage( LPCTSTR sPath) : m_sPath( sPath) { }

	BEGIN_DLGRESIZE_MAP(CMoveDlg)
		BEGIN_DLGRESIZE_GROUP()
			DLGRESIZE_CONTROL(IDC_TYPENAME_H, DLSZ_MOVE_X | DLSZ_MOVE_Y)
			DLGRESIZE_CONTROL(IDC_TYPENAME, DLSZ_MOVE_X | DLSZ_MOVE_Y)
			DLGRESIZE_CONTROL(IDC_FILEICON, DLSZ_MOVE_X | DLSZ_MOVE_Y)
			DLGRESIZE_CONTROL(IDC_FILELOCATION_H, DLSZ_MOVE_X | DLSZ_MOVE_Y)
			DLGRESIZE_CONTROL(IDC_FILELOCATION, DLSZ_MOVE_X | DLSZ_MOVE_Y)
			DLGRESIZE_CONTROL(IDC_FILENAME_H, DLSZ_MOVE_X | DLSZ_MOVE_Y)
			DLGRESIZE_CONTROL(IDC_FILENAME, DLSZ_MOVE_X | DLSZ_MOVE_Y)
			DLGRESIZE_CONTROL(IDC_FILESIZE_H, DLSZ_MOVE_X | DLSZ_MOVE_Y)
			DLGRESIZE_CONTROL(IDC_FILESIZE, DLSZ_MOVE_X | DLSZ_MOVE_Y)
			DLGRESIZE_CONTROL(IDC_FILEDATE_H, DLSZ_MOVE_X | DLSZ_MOVE_Y)
			DLGRESIZE_CONTROL(IDC_FILEDATE, DLSZ_MOVE_X | DLSZ_MOVE_Y)
			DLGRESIZE_CONTROL(IDC_FILEATTRIB_H, DLSZ_MOVE_X | DLSZ_MOVE_Y)
			DLGRESIZE_CONTROL(IDC_FILEATTRIB, DLSZ_MOVE_X | DLSZ_MOVE_Y)
		END_DLGRESIZE_GROUP()
	END_DLGRESIZE_MAP()

	BEGIN_MSG_MAP(CFilePage)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		MESSAGE_HANDLER(WM_SIZE, CDialogResize<CFilePage>::OnSize)
		CHAIN_MSG_MAP(CPropertyPageImpl<CFilePage>)
	END_MSG_MAP()

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	{
		ATLASSERT( !m_sPath.IsEmpty()); 

		CString s;


		SHFILEINFO sfi;
		::SHGetFileInfo( m_sPath, 0, &sfi, sizeof(sfi), SHGFI_ICON | SHGFI_TYPENAME);
		SendDlgItemMessage( IDC_FILEICON, STM_SETIMAGE, IMAGE_ICON, (LPARAM)sfi.hIcon);
		SetDlgItemText( IDC_TYPENAME, sfi.szTypeName);
		
		s = m_sPath.Left( m_sPath.ReverseFind(L'\\'));

		SetDlgItemText( IDC_FILELOCATION, s);
		SetDlgItemText( IDC_FILENAME, m_sPath.Mid( m_sPath.ReverseFind(L'\\') + 1));

		CFindFile ff;
		ff.FindFile( m_sPath);
		SetDlgItemInt( IDC_FILESIZE, (UINT)ff.GetFileSize());

		FILETIME ftim;
		ff.GetCreationTime( &ftim);
		FormatFileTime( &ftim, s);
		SetDlgItemText( IDC_FILEDATE, s);

		s.Empty();
		if ( ff.MatchesMask( FILE_ATTRIBUTE_ENCRYPTED))
			s += L"Encrypted, ";
		if ( ff.IsCompressed())
			s += L"Compressed, ";
		if ( ff.IsArchived())
			s += L"Archive, ";
		if ( ff.IsReadOnly())
			s += L"Read-only";
		
		if ( s.Find( L", ", s.GetLength() - 2) != -1)
			s = s.Left( s.GetLength() - 2);
		SetDlgItemText( IDC_FILEATTRIB, s);

		DlgResize_Init(FALSE);
		return TRUE;
	}

// Implementation 
	void FormatFileTime( FILETIME *pftim, CString& sDateTime)
	{
		SYSTEMTIME stim;
		CString s;
		::FileTimeToSystemTime( pftim, &stim);
		::GetDateFormat( LOCALE_USER_DEFAULT, 0, &stim, NULL, sDateTime.GetBuffer(40), 40);
		sDateTime.ReleaseBuffer();
		::GetTimeFormat( LOCALE_USER_DEFAULT, TIME_NOSECONDS, &stim, NULL, s.GetBuffer(40), 40);
		s.ReleaseBuffer();
		sDateTime += L"  " + s;
	}

};

/////////////////////
// CImagePage: Current image properties 

class CImagePage : public CPropertyPageImpl<CImagePage>,public CDialogResize<CImagePage>
{
public:
	enum { IDD = IDD_PROP_IMAGE };
	CBitmapHandle m_bmp;
	
	CImagePage(HBITMAP hbmp) : m_bmp(hbmp) {}

	BEGIN_DLGRESIZE_MAP(CMoveDlg)
		BEGIN_DLGRESIZE_GROUP()
			DLGRESIZE_CONTROL(IDC_TYPENAME_H, DLSZ_MOVE_X | DLSZ_MOVE_Y)
			DLGRESIZE_CONTROL(IDC_TYPENAME, DLSZ_MOVE_X | DLSZ_MOVE_Y)
			DLGRESIZE_CONTROL(IDC_IMAGESIZE_H, DLSZ_MOVE_X | DLSZ_MOVE_Y)
			DLGRESIZE_CONTROL(IDC_IMAGESIZE, DLSZ_MOVE_X | DLSZ_MOVE_Y)
			DLGRESIZE_CONTROL(IDC_NUMCOLORS_H, DLSZ_MOVE_X | DLSZ_MOVE_Y)
			DLGRESIZE_CONTROL(IDC_NUMCOLORS, DLSZ_MOVE_X | DLSZ_MOVE_Y)
			DLGRESIZE_CONTROL(IDC_BITDEPTH_H, DLSZ_MOVE_X | DLSZ_MOVE_Y)
			DLGRESIZE_CONTROL(IDC_BITDEPTH, DLSZ_MOVE_X | DLSZ_MOVE_Y)
			DLGRESIZE_CONTROL(IDC_NUMBYTES_H, DLSZ_MOVE_X | DLSZ_MOVE_Y)
			DLGRESIZE_CONTROL(IDC_NUMBYTES, DLSZ_MOVE_X | DLSZ_MOVE_Y)
			DLGRESIZE_CONTROL(IDC_IMAGE, DLSZ_MOVE_X | DLSZ_MOVE_Y | DLSZ_REPAINT)
		END_DLGRESIZE_GROUP()
	END_DLGRESIZE_MAP()

	BEGIN_MSG_MAP(CImagePage)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		MESSAGE_HANDLER(WM_SIZE, CDialogResize<CImagePage>::OnSize)
		CHAIN_MSG_MAP(CPropertyPageImpl<CImagePage>)
	END_MSG_MAP()

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	{
		ATLASSERT( !m_bmp.IsNull());

		DIBSECTION ds;
		ATLVERIFY( ::GetObject( m_bmp, sizeof(DIBSECTION), &ds) == sizeof(DIBSECTION));

		CString s;
		s.Format( L"(h) %d x (v) %d", ds.dsBmih.biWidth, ds.dsBmih.biHeight);
		SetDlgItemText( IDC_IMAGESIZE, s);
		SetDlgItemInt( IDC_BITDEPTH, ds.dsBmih.biBitCount);
		SetDlgItemInt( IDC_NUMBYTES, ds.dsBmih.biSizeImage);
		SetDlgItemInt( IDC_NUMCOLORS, AtlGetDibNumColors( &ds.dsBmih));

		CStatic sImg = GetDlgItem( IDC_IMAGE);
		CRect rectImg;
		sImg.GetWindowRect( rectImg);
		CSize sizeImg( ds.dsBmih.biWidth, ds.dsBmih.biHeight);
		double fzoom = max( (double)sizeImg.cx / rectImg.Width(),
						(double)sizeImg.cy / rectImg.Height());

		CBitmapHandle hbm = AtlCopyBitmap( m_bmp, sizeImg / fzoom, true);
		sImg.SetBitmap( hbm);

		DlgResize_Init(FALSE);
		return TRUE;
	}
};

/////////////////////
// CViewPage: Current view properties 

class CViewPage : public CPropertyPageImpl<CViewPage>
{
public:

	CImageViewView& m_rview;
	
	CViewPage( CImageViewView& rview) : m_rview( rview){}

	enum { IDD = IDD_PROP_VIEW };

	BEGIN_MSG_MAP(CViewPage)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		CHAIN_MSG_MAP(CPropertyPageImpl<CViewPage>)
	END_MSG_MAP()

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	{
		CString s;
		s.Format( _T("%.2f"), m_rview.GetZoom());
		SetDlgItemText( IDC_ZOOM, s);
		s.Format(_T("(h) %d x (v) %d"), m_rview.m_sizeAll.cx, m_rview.m_sizeAll.cy);
		SetDlgItemText( IDC_IMAGESIZE, s);

		CStatic sImg = GetDlgItem( IDC_VIEW);
		CRect rectImg;
		sImg.GetWindowRect( rectImg);
		double zoom = max( (double)m_rview.GetScrollSize().cx / rectImg.Width(),
						(double)m_rview.GetScrollSize().cy / rectImg.Height());
		CBitmapHandle hbm = AtlCopyBitmap( m_rview.m_bmp, m_rview.GetScrollSize() / zoom, true);

// Draw view rectangle in image 
		CDC dc = CreateCompatibleDC( NULL);
		dc.SelectStockBrush( HOLLOW_BRUSH);
		HBITMAP bmpOld = dc.SelectBitmap( hbm)
			;
		CRect rect( CPoint( 0, 0), m_rview.m_sizeClient);
		m_rview.WndtoTrue( rect);
		rect = CRect( (CPoint)( CSize( rect.TopLeft()) / zoom), rect.Size() / zoom);
		CPen pen;
		pen.CreatePen( PS_SOLID, 2, RGB( 255, 0,  0));
		HPEN penOld=dc.SelectPen( pen);
		dc.Rectangle( rect);

		dc.SelectPen( penOld);
		dc.SelectBitmap( bmpOld);
		sImg.SetBitmap( hbm);
		return TRUE;
	}

};

class CImageProperties : public CPropertySheetImpl<CImageProperties>
{
public:
	CFilePage m_FilePage;
	CImagePage m_ImagePage;
	CViewPage m_ViewPage;

	CImageProperties( LPCTSTR sname, CImageViewView & rview) :
	m_FilePage(sname), m_ImagePage( rview.m_bmp), m_ViewPage( rview) 
	{
		SetTitle( rview.m_sImageName);

		if ( *sname )
			AddPage( m_FilePage);
		AddPage( m_ImagePage);
		AddPage( m_ViewPage);
	}

	void OnSheetInitialized()
	{
		AtlCreateEmptyMenuBar(m_hWnd, false);
	}
};

#endif // _IMAGEVIEW_DLG_H__
