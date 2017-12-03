//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/resource.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__RESOURCE_H
#define _ASPIA_UI__RESOURCE_H

#include <windows.h>
#include <commctrl.h>
#include <richedit.h>
#include <atlres.h>

//
// Icons
//
#define IDI_MAIN                                   100
#define IDI_ABOUT                                  101
#define IDI_APPLICATIONS                           102
#define IDI_AUTOSIZE                               103
#define IDI_BATTERY                                104
#define IDI_BIOS                                   105
#define IDI_BOOKS_STACK                            106
#define IDI_CAD                                    107
#define IDI_CHECKED                                108
#define IDI_CHIP                                   109
#define IDI_CLAPPERBOARD                           110
#define IDI_CLOCK                                  111
#define IDI_COMPUTER                               112
#define IDI_COUNTER                                113
#define IDI_DELETE                                 114
#define IDI_DISK                                   115
#define IDI_DOCUMENT_TEXT                          116
#define IDI_DRIVE                                  117
#define IDI_DRIVE_DISK                             118
#define IDI_ERROR_LOG                              119
#define IDI_EXIT                                   120
#define IDI_FOLDER                                 121
#define IDI_FOLDER_ADD                             122
#define IDI_FOLDER_UP                              123
#define IDI_FOLDER_NETWORK                         124
#define IDI_FULLSCREEN                             125
#define IDI_GEAR                                   126
#define IDI_HARDWARE                               127
#define IDI_HOME                                   128
#define IDI_KEYS                                   129
#define IDI_LICENSE_KEY                            130
#define IDI_LIST                                   131
#define IDI_MEMORY                                 132
#define IDI_MINUS                                  133
#define IDI_MONITOR                                134
#define IDI_MOTHERBOARD                            135
#define IDI_MOUSE                                  136
#define IDI_NETWORK                                137
#define IDI_NETWORK_ADAPTER                        138
#define IDI_NETWORK_IP                             139
#define IDI_OS                                     140
#define IDI_PCI                                    141
#define IDI_PENCIL                                 142
#define IDI_PLUS                                   143
#define IDI_PORT                                   144
#define IDI_POWER_SURGE                            145
#define IDI_POWER_SUPPLY                           146
#define IDI_PRINTER                                147
#define IDI_PRINTER_SHARE                          148
#define IDI_PROCESSOR                              149
#define IDI_RECIEVE                                150
#define IDI_REFRESH                                151
#define IDI_ROUTE                                  152
#define IDI_SELECT_ALL                             153
#define IDI_SEND                                   154
#define IDI_SERVER                                 155
#define IDI_SERVERS_NETWORK                        156
#define IDI_SETTINGS                               157
#define IDI_SOFTWARE                               158
#define IDI_SYSTEM_MONITOR                         159
#define IDI_TELEPHONE_FAX                          160
#define IDI_TREE                                   161
#define IDI_UNCHECKED                              162
#define IDI_UNSELECT_ALL                           163
#define IDI_USER                                   164
#define IDI_USER_DISABLED                          165
#define IDI_USERS                                  166

//
// Dialogs
//
#define IDD_MAIN                                   100
#define IDD_AUTH                                   101
#define IDD_ABOUT                                  102
#define IDD_STATUS                                 103
#define IDD_SETTINGS                               104
#define IDD_USERS                                  105
#define IDD_USER_PROP                              106
#define IDD_POWER                                  107
#define IDD_FILE_PROGRESS                          108
#define IDD_FILE_ACTION                            109
#define IDD_FILE_STATUS                            110
#define IDD_POWER_HOST                             111
#define IDD_CATEGORY_SELECT                        112
#define IDD_REPORT_PROGRESS                        113

//
// Controls
//
#define IDC_SERVER_ADDRESS_EDIT                    100
#define IDC_SERVER_PORT_EDIT                       101
#define IDC_SERVER_DEFAULT_PORT_CHECK              102
#define IDC_SETTINGS_BUTTON                        103
#define IDC_CONNECT_BUTTON                         104
#define IDC_IP_LIST                                105
#define IDC_START_SERVER_BUTTON                    106
#define IDC_ABOUT_ICON                             107
#define IDC_VERSION_TEXT                           108
#define IDC_ABOUT_EDIT                             109
#define IDC_DONATE_BUTTON                          110
#define IDC_SITE_BUTTON                            111
#define IDC_STATUS_EDIT                            112
#define IDC_USERNAME_EDIT                          113
#define IDC_PASSWORD_EDIT                          114
#define IDC_PASSWORD_RETRY_EDIT                    115
#define IDC_CODEC_COMBO                            116
#define IDC_COLOR_DEPTH_COMBO                      117
#define IDC_COMPRESS_RATIO_TEXT                    118
#define IDC_COMPRESS_RATIO_TRACKBAR                119
#define IDC_ENABLE_CURSOR_SHAPE_CHECK              120
#define IDC_INTERVAL_EDIT                          121
#define IDC_INTERVAL_UPDOWN                        122
#define IDC_ENABLE_CLIPBOARD_CHECK                 123
#define IDC_FAST_TEXT                              124
#define IDC_BEST_TEXT                              125
#define IDC_COLOR_DEPTH_TEXT                       126
#define IDC_USER_LIST                              127
#define IDC_DISABLE_USER_CHECK                     128
#define IDC_SESSION_TYPES_LIST                     129
#define IDC_SESSION_TYPE_COMBO                     130
#define IDC_CURRENT_ITEM_EDIT                      131
#define IDC_TOTAL_PROGRESS                         132
#define IDC_CURRENT_PROGRESS                       133
#define IDC_REPLACE_BUTTON                         134
#define IDC_REPLACE_ALL_BUTTON                     135
#define IDC_SKIP_BUTTON                            136
#define IDC_SKIP_ALL_BUTTON                        137
#define IDC_STOP_BUTTON                            138
#define IDC_MINIMIZE_BUTTON                        139
#define IDC_BUTTON_GROUP                           140
#define IDC_POWER_ICON                             141
#define IDC_POWER_ACTION                           142
#define IDC_POWER_TIME                             143
#define IDC_FILE_MANAGER_ACCELERATORS              144
#define IDC_UPDATE_IP_LIST_BUTTON                  145
#define IDC_CATEGORY_TREE                          146
#define IDC_SELECT_ALL                             147
#define IDC_UNSELECT_ALL                           148
#define IDC_CURRENT_CATEGORY                       149
#define IDC_CURRENT_ACTION                         150
#define IDC_REPORT_PROGRESS                        151
#define IDC_SYSTEM_INFO_ACCELERATORS               152
#define IDC_EXPAND_ALL                             153
#define IDC_COLLAPSE_ALL                           154

//
// Commands
//
#define ID_CAD                                     500
#define ID_SHORTCUTS                               501
#define ID_AUTO_SIZE                               502
#define ID_FULLSCREEN                              503
#define ID_SETTINGS                                504
#define ID_SHOWHIDE                                505
#define ID_INSTALL_SERVICE                         506
#define ID_REMOVE_SERVICE                          507
#define ID_USERS                                   508
#define ID_ABOUT                                   509
#define ID_EXIT                                    510
#define ID_EDIT                                    511
#define ID_DELETE                                  512
#define ID_ADD                                     513
#define ID_REFRESH                                 514
#define ID_FOLDER_ADD                              515
#define ID_FOLDER_UP                               516
#define ID_HOME                                    517
#define ID_SEND                                    518
#define ID_COPY                                    519
#define ID_SAVE                                    520
#define ID_SAVE_ALL                                521
#define ID_SAVE_SELECTED                           522
#define ID_SAVE_CURRENT                            523
#define ID_COPY_ALL                                524
#define ID_COPY_VALUE                              525

#define ID_KEY_FIRST                               600
#define ID_KEY_CTRL_ESC                            600
#define ID_KEY_ALT_TAB                             601
#define ID_KEY_ALT_SHIFT_TAB                       602
#define ID_KEY_PRINTSCREEN                         603
#define ID_KEY_ALT_PRINTSCREEN                     604
#define ID_KEY_CTRL_ALT_F12                        605
#define ID_KEY_F12                                 606
#define ID_KEY_CTRL_F12                            607
#define ID_KEY_LAST                                607

#define ID_POWER_SHUTDOWN                          608
#define ID_POWER_REBOOT                            609
#define ID_POWER_HIBERNATE                         610
#define ID_POWER_SUSPEND                           611

#define ID_SYSTEM_INFO                             700

//
// Menu
//
#define IDR_SHORTCUTS                              100
#define IDR_TRAY                                   101
#define IDR_MAIN                                   102
#define IDR_USER                                   103
#define IDR_IP_LIST                                104
#define IDR_SAVE_REPORT                            105
#define IDR_LIST_COPY                              106

//
// Strings
//

// General
#define IDS_APPLICATION_NAME                        1
#define IDS_START                                   2
#define IDS_STOP                                    3
#define IDS_DONATE_LINK                             4
#define IDS_SITE_LINK                               5
#define IDS_HELP_LINK                               6
#define IDS_ABOUT_STRING                            7
#define IDS_CONFIRMATION                            8
#define IDS_CONNECTION                              9
#define IDS_INVALID_USERNAME                        10
#define IDS_USER_ALREADY_EXISTS                     11
#define IDS_INVALID_PASSWORD                        12
#define IDS_PASSWORDS_NOT_MATCH                     13
#define IDS_SESSION_TYPE_DESKTOP_MANAGE             14
#define IDS_SESSION_TYPE_DESKTOP_VIEW               15
#define IDS_SESSION_TYPE_POWER_MANAGE               16
#define IDS_SESSION_TYPE_FILE_TRANSFER              17
#define IDS_SESSION_TYPE_SYSTEM_INFO                18
#define IDS_DELETE_USER_CONFORMATION                19
#define IDS_USER_LIST_MODIFIED                      20
#define IDS_USER_LIST                               21

// Desktop Manage
#define IDS_DM_TOOLTIP_AUTO_SIZE                    500
#define IDS_DM_TOOLTIP_SETTINGS                     501
#define IDS_DM_TOOLTIP_CAD                          502
#define IDS_DM_TOOLTIP_EXIT                         503
#define IDS_DM_TOOLTIP_FULLSCREEN                   504
#define IDS_DM_TOOLTIP_SHORTCUTS                    505
#define IDS_DM_TOOLTIP_ABOUT                        506
#define IDS_DM_COMPRESSION_RATIO_FORMAT             507
#define IDS_DM_32BIT                                508
#define IDS_DM_24BIT                                509
#define IDS_DM_16BIT                                510
#define IDS_DM_15BIT                                511
#define IDS_DM_12BIT                                512
#define IDS_DM_8BIT                                 513
#define IDS_DM_6BIT                                 514
#define IDS_DM_3BIT                                 515

// File transfer
#define IDS_FT_FILE_TRANSFER                        1000
#define IDS_FT_LOCAL_COMPUTER                       1001
#define IDS_FT_REMOTE_COMPUTER                      1002
#define IDS_FT_COLUMN_NAME                          1003
#define IDS_FT_COLUMN_SIZE                          1004
#define IDS_FT_COLUMN_TYPE                          1005
#define IDS_FT_COLUMN_MODIFIED                      1006
#define IDS_FT_COLUMN_TOTAL_SPACE                   1007
#define IDS_FT_COLUMN_FREE_SPACE                    1008
#define IDS_FT_TOOLTIP_REFRESH                      1009
#define IDS_FT_TOOLTIP_DELETE                       1010
#define IDS_FT_TOOLTIP_FOLDER_ADD                   1011
#define IDS_FT_TOOLTIP_FOLDER_UP                    1012
#define IDS_FT_TOOLTIP_HOME                         1013
#define IDS_FT_TOOLTIP_SEND                         1014
#define IDS_FT_TOOLTIP_RECIEVE                      1015
#define IDS_FT_HOME_FOLDER                          1016
#define IDS_FT_DESKTOP_FOLDER                       1017
#define IDS_FT_SIZE_TBYTES                          1018
#define IDS_FT_SIZE_GBYTES                          1019
#define IDS_FT_SIZE_MBYTES                          1020
#define IDS_FT_SIZE_KBYTES                          1021
#define IDS_FT_SIZE_BYTES                           1022
#define IDS_FT_NEW_FOLDER                           1023
#define IDS_FT_COMPUTER                             1024
#define IDS_FT_DRIVE_DESC_DESKTOP                   1025
#define IDS_FT_DRIVE_DESC_HOME                      1026
#define IDS_FT_DRIVE_DESC_CDROM                     1027
#define IDS_FT_DRIVE_DESC_FIXED                     1028
#define IDS_FT_DRIVE_DESC_REMOVABLE                 1029
#define IDS_FT_DRIVE_DESC_REMOTE                    1030
#define IDS_FT_DRIVE_DESC_RAM                       1031
#define IDS_FT_DRIVE_DESC_UNKNOWN                   1032
#define IDS_FT_DELETE_CONFORM                       1033
#define IDS_FT_OP_SESSION_START                     1034
#define IDS_FT_OP_SESSION_END                       1035
#define IDS_FT_OP_BROWSE_FOLDERS                    1036
#define IDS_FT_OP_RENAME                            1037
#define IDS_FT_OP_REMOVE                            1038
#define IDS_FT_OP_CREATE_FOLDER                     1039
#define IDS_FT_OP_BROWSE_DRIVES                     1040
#define IDS_FT_OP_SEND_FILE                         1041
#define IDS_FT_OP_RECIEVE_FILE                      1042
#define IDS_FT_SEND                                 1043
#define IDS_FT_RECIEVE                              1044
#define IDS_FT_SELECTED_OBJECT_COUNT                1045
#define IDS_FT_BROWSE_FOLDERS_ERROR                 1046
#define IDS_FT_OP_RENAME_ERROR                      1047
#define IDS_FT_OP_CREATE_FOLDER_ERROR               1048
#define IDS_FT_OP_BROWSE_DRIVES_ERROR               1049
#define IDS_FT_FILE_FOLDER                          1050
#define IDS_FT_CLOSE_WINDOW                         1051
#define IDS_FT_OP_FAILURE                           1052
#define IDS_FT_FILE_LIST_BUILDING                   1053

// Power Manage
#define IDS_PM_SHUTDOWN_COMMAND                     2000
#define IDS_PM_REBOOT_COMMAND                       2001
#define IDS_PM_HIBERNATE_COMMAND                    2002
#define IDS_PM_SUSPEND_COMMAND                      2003
#define IDS_PM_TIME_LEFT                            2004

// System Information
#define IDS_SI_SYSTEM_INFORMATION                   3000
#define IDS_SI_SAVE_REPORT                          3001
#define IDS_SI_TOOLTIP_SAVE                         3002
#define IDS_SI_TOOLTIP_HOME                         3003
#define IDS_SI_TOOLTIP_REFRESH                      3004
#define IDS_SI_TOOLTIP_ABOUT                        3005
#define IDS_SI_TOOLTIP_EXIT                         3006
#define IDS_SI_STATE_REQUEST                        3007
#define IDS_SI_STATE_OUTPUT                         3008
#define IDS_SI_FILTER_HTML                          3009
#define IDS_SI_FILTER_JSON                          3010
#define IDS_SI_FILTER_XML                           3011

// Request Status
#define IDS_REQUEST_STATUS_FIRST                    11000
#define IDS_REQUEST_STATUS_UNKNOWN                  11000
#define IDS_REQUEST_STATUS_SUCCESS                  11001
#define IDS_REQUEST_STATUS_INVALID_PATH_NAME        11002
#define IDS_REQUEST_STATUS_PATH_NOT_FOUND           11003
#define IDS_REQUEST_STATUS_PATH_ALREADY_EXISTS      11004
#define IDS_REQUEST_STATUS_NO_DRIVES_FOUND          11005
#define IDS_REQUEST_STATUS_DISK_FULL                11006
#define IDS_REQUEST_STATUS_ACCESS_DENIED            11007
#define IDS_REQUEST_STATUS_FILE_OPEN_ERROR          11008
#define IDS_REQUEST_STATUS_FILE_CREATE_ERROR        11009
#define IDS_REQUEST_STATUS_FILE_WRITE_ERROR         11010
#define IDS_REQUEST_STATUS_FILE_READ_ERROR          11011
#define IDS_REQUEST_STATUS_LAST                     11011

// Status
#define IDS_STATUS_FIRST                            10000
#define IDS_STATUS_SUCCESS                          10000
#define IDS_STATUS_NO_CONSOLE_SESSION               10001
#define IDS_STATUS_ACCESS_DENIED                    10002
#define IDS_STATUS_INVALID_ADDRESS                  10003
#define IDS_STATUS_INVALID_PORT                     10004
#define IDS_STATUS_CONNECT_TIMEOUT                  10005
#define IDS_STATUS_CONNECT_ERROR                    10006
#define IDS_STATUS_CONNECTING                       10007
#define IDS_STATUS_LAST                             10007
#define IDS_STATUS_UNKNOWN                          10009

#endif // _ASPIA_UI__RESOURCE_H
