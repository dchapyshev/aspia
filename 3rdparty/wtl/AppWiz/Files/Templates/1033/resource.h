//{{NO_DEPENDENCIES}}
// Microsoft Visual C++ generated include file.
// Used by [!output PROJECT_NAME].RC
//

[!if WTL_COM_SERVER]
#define IDS_PROJNAME				100
#define IDR_[!output UPPERCASE_SAFE_PROJECT_NAME]	100
[!endif]

#define IDD_ABOUTBOX				100
#define IDR_MAINFRAME				128
//#define IDR_[!output UPPERCASE_SAFE_PROJECT_NAME]TYPE	129
[!if WTL_APPTYPE_MDI]
#define IDR_MDICHILD				129
[!if WTL_USE_VIEW]
[!if WTL_VIEWTYPE_FORM]
#define IDD_[!output UPPERCASE_SAFE_PROJECT_NAME]_FORM	130
[!endif]
[!endif]
[!else]
[!if WTL_APPTYPE_SDI || WTL_APPTYPE_TABVIEW || WTL_APPTYPE_EXPLORER]
[!if WTL_USE_VIEW]
[!if WTL_VIEWTYPE_FORM]
#define IDD_[!output UPPERCASE_SAFE_PROJECT_NAME]_FORM	129
[!endif]
[!endif]
[!else]
[!if WTL_APPTYPE_DLG]
#define IDD_MAINDLG				129
[!endif]
[!endif]
[!endif]
[!if WTL_APPTYPE_MTSDI]
[!if WTL_USE_VIEW]
[!if WTL_VIEWTYPE_FORM]
#define IDD_[!output UPPERCASE_SAFE_PROJECT_NAME]_FORM	130
[!endif]
[!endif]
#define ID_FILE_NEW_WINDOW			32771
[!endif]
[!if WTL_APPTYPE_TABVIEW]
#define ID_WINDOW_CLOSE				32772
#define ID_WINDOW_CLOSE_ALL			32773
[!endif]
[!if WTL_APPTYPE_EXPLORER]
#define ID_VIEW_TREEPANE			32774
[!endif]

// Next default values for new objects
// 
#ifdef APSTUDIO_INVOKED
#ifndef APSTUDIO_READONLY_SYMBOLS
#define _APS_NEXT_RESOURCE_VALUE	201
#define _APS_NEXT_CONTROL_VALUE		1000
#define _APS_NEXT_SYMED_VALUE		101
#define _APS_NEXT_COMMAND_VALUE		32775
#endif
#endif
