//{{NO_DEPENDENCIES}}
// Microsoft Visual C++ generated include file.
//?ppc
// Used by [!output PROJECT_NAME]ppc.RC
//?sp
// Used by [!output PROJECT_NAME]sp.RC
//?end
//

#define IDD_ABOUTBOX				100
#define IDR_MAINFRAME				128
[!if WTL_APPTYPE_SDI]
[!if WTL_USE_VIEW]
[!if WTL_VIEWTYPE_FORM]
#define IDD_[!output UPPERCASE_SAFE_PROJECT_NAME]_FORM	129
[!endif]
[!if WTL_VIEWTYPE_PROPSHEET]
#define IDD_[!output UPPERCASE_SAFE_PROJECT_NAME]_PAGE	129
[!endif]
[!endif]
[!else]
[!if WTL_APPTYPE_DLG]
#define IDD_MAINDLG				129
[!if WTL_APP_DLG_ORIENT]
#define IDD_MAINDLG_L			130
[!endif]
[!endif]
[!endif]

// Next default values for new objects
// 
#ifdef APSTUDIO_INVOKED
#ifndef APSTUDIO_READONLY_SYMBOLS
#define _APS_NEXT_RESOURCE_VALUE	201
#define _APS_NEXT_CONTROL_VALUE		1000
#define _APS_NEXT_SYMED_VALUE		101
#define _APS_NEXT_COMMAND_VALUE		32772
#endif
#endif
