//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/resource.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__RESOURCE_H
#define _ASPIA_UI__RESOURCE_H

#include <windows.h>
#include <commctrl.h>
#include <atlres.h>

//
// Icons
//
#define IDI_MAIN                              100
#define IDI_ABOUT                             101
#define IDI_CAD                               102
#define IDI_DELETE                            103
#define IDI_EXIT                              104
#define IDI_FOLDER_ADD                        105
#define IDI_FOLDER_UP                         106
#define IDI_FULLSCREEN                        107
#define IDI_HOME                              108
#define IDI_AUTOSIZE                          109
#define IDI_KEYS                              110
#define IDI_POWER                             111
#define IDI_RECIEVE                           112
#define IDI_REFRESH                           113
#define IDI_SEND                              114
#define IDI_SETTINGS                          115
#define IDI_USER                              116
#define IDI_USER_DISABLED                     117

//
// Dialogs
//
#define IDD_MAIN                              100
#define IDD_AUTH                              101
#define IDD_ABOUT                             102
#define IDD_STATUS                            103
#define IDD_SETTINGS                          104
#define IDD_USERS                             105
#define IDD_USER_PROP                         106
#define IDD_POWER                             107
#define IDD_FILE_TRANSFER                     108
#define IDD_FILE_REPLACE                      109
#define IDD_FILE_STATUS                       110

//
// Controls
//
#define IDC_SERVER_ADDRESS_EDIT               100
#define IDC_SERVER_PORT_EDIT                  101
#define IDC_SERVER_DEFAULT_PORT_CHECK         102
#define IDC_SETTINGS_BUTTON                   103
#define IDC_CONNECT_BUTTON                    104
#define IDC_IP_LIST                           105
#define IDC_START_SERVER_BUTTON               106
#define IDC_ABOUT_ICON                        107
#define IDC_VERSION_TEXT                      108
#define IDC_ABOUT_EDIT                        109
#define IDC_DONATE_BUTTON                     110
#define IDC_SITE_BUTTON                       111
#define IDC_STATUS_EDIT                       112
#define IDC_USERNAME_EDIT                     113
#define IDC_PASSWORD_EDIT                     114
#define IDC_PASSWORD_RETRY_EDIT               115
#define IDC_CODEC_COMBO                       116
#define IDC_COLOR_DEPTH_COMBO                 117
#define IDC_COMPRESS_RATIO_TEXT               118
#define IDC_COMPRESS_RATIO_TRACKBAR           119
#define IDC_ENABLE_CURSOR_SHAPE_CHECK         120
#define IDC_INTERVAL_EDIT                     121
#define IDC_INTERVAL_UPDOWN                   122
#define IDC_ENABLE_CLIPBOARD_CHECK            123
#define IDC_FAST_TEXT                         124
#define IDC_BEST_TEXT                         125
#define IDC_COLOR_DEPTH_TEXT                  126
#define IDC_USER_LIST                         127
#define IDC_DISABLE_USER_CHECK                128
#define IDC_SESSION_TYPES_LIST                129
#define IDC_USERS_GROUPBOX                    130
#define IDC_SESSION_TYPE_COMBO                131
#define IDC_FROM_EDIT                         132
#define IDC_TO_EDIT                           133
#define IDC_CURRENT_ITEM_EDIT                 134
#define IDC_TOTAL_PROGRESS_TEXT               135
#define IDC_TOTAL_PROGRESS                    136
#define IDC_CURRENT_PROGRESS_TEXT             137
#define IDC_CURRENT_PROGRESS                  138
#define IDC_REPLACE_BUTTON                    139
#define IDC_REPLACE_ALL_BUTTON                140
#define IDC_SKIP_BUTTON                       141
#define IDC_SKIP_ALL_BUTTON                   142
#define IDC_STOP_BUTTON                       143
#define IDC_MINIMIZE_BUTTON                   144

//
// Commands
//
#define ID_POWER                              500
#define ID_CAD                                501
#define ID_SHORTCUTS                          502
#define ID_AUTO_SIZE                          503
#define ID_FULLSCREEN                         504
#define ID_SETTINGS                           505
#define ID_SHOWHIDE                           506
#define ID_INSTALL_SERVICE                    507
#define ID_REMOVE_SERVICE                     508
#define ID_USERS                              509
#define ID_ABOUT                              510
#define ID_EXIT                               511
#define ID_EDIT                               512
#define ID_DELETE                             513
#define ID_ADD                                514
#define ID_REFRESH                            515
#define ID_FOLDER_ADD                         516
#define ID_FOLDER_UP                          517
#define ID_HOME                               518
#define ID_SEND                               519

#define ID_KEY_FIRST                          600
#define ID_KEY_CTRL_ESC                       600
#define ID_KEY_ALT_TAB                        601
#define ID_KEY_ALT_SHIFT_TAB                  602
#define ID_KEY_PRINTSCREEN                    603
#define ID_KEY_ALT_PRINTSCREEN                604
#define ID_KEY_CTRL_ALT_F12                   605
#define ID_KEY_F12                            606
#define ID_KEY_CTRL_F12                       607
#define ID_KEY_LAST                           607

#define ID_POWER_SHUTDOWN                     608
#define ID_POWER_REBOOT                       609
#define ID_POWER_HIBERNATE                    610
#define ID_POWER_SUSPEND                      611

//
// Menu
//
#define IDR_SHORTCUTS                         100
#define IDR_TRAY                              101
#define IDR_MAIN                              102
#define IDR_USER                              103

//
// Strings
//

// General
#define IDS_APPLICATION_NAME                  1
#define IDS_START                             2
#define IDS_STOP                              3
#define IDS_DONATE_LINK                       4
#define IDS_SITE_LINK                         5
#define IDS_HELP_LINK                         6
#define IDS_ABOUT_STRING                      7
#define IDS_CONFIRMATION                      8
#define IDS_CONNECTION                        9
#define IDS_INVALID_USERNAME                  10
#define IDS_USER_ALREADY_EXISTS               11
#define IDS_INVALID_PASSWORD                  12
#define IDS_PASSWORDS_NOT_MATCH               13
#define IDS_SESSION_TYPE_DESKTOP_MANAGE       14
#define IDS_SESSION_TYPE_DESKTOP_VIEW         15
#define IDS_SESSION_TYPE_POWER_MANAGE         16
#define IDS_SESSION_TYPE_FILE_TRANSFER        17
#define IDS_SESSION_TYPE_SYSTEM_INFO          18
#define IDS_DELETE_USER_CONFORMATION          19
#define IDS_USER_LIST_MODIFIED                20

// Desktop Manage
#define IDS_DM_TOOLTIP_POWER                  500
#define IDS_DM_TOOLTIP_AUTO_SIZE              501
#define IDS_DM_TOOLTIP_SETTINGS               502
#define IDS_DM_TOOLTIP_CAD                    503
#define IDS_DM_TOOLTIP_EXIT                   504
#define IDS_DM_TOOLTIP_FULLSCREEN             505
#define IDS_DM_TOOLTIP_SHORTCUTS              506
#define IDS_DM_TOOLTIP_ABOUT                  507
#define IDS_DM_COMPRESSION_RATIO_FORMAT       508
#define IDS_DM_32BIT                          509
#define IDS_DM_24BIT                          510
#define IDS_DM_16BIT                          511
#define IDS_DM_15BIT                          512
#define IDS_DM_12BIT                          513
#define IDS_DM_8BIT                           514
#define IDS_DM_6BIT                           515
#define IDS_DM_3BIT                           516

// File transfer
#define IDS_FT_FILE_TRANSFER                  1000
#define IDS_FT_LOCAL_COMPUTER                 1001
#define IDS_FT_REMOTE_COMPUTER                1002
#define IDS_FT_COLUMN_NAME                    1003
#define IDS_FT_COLUMN_SIZE                    1004
#define IDS_FT_COLUMN_TYPE                    1005
#define IDS_FT_COLUMN_MODIFIED                1006
#define IDS_FT_COLUMN_TOTAL_SPACE             1007
#define IDS_FT_COLUMN_FREE_SPACE              1008
#define IDS_FT_TOOLTIP_REFRESH                1009
#define IDS_FT_TOOLTIP_DELETE                 1010
#define IDS_FT_TOOLTIP_FOLDER_ADD             1011
#define IDS_FT_TOOLTIP_FOLDER_UP              1012
#define IDS_FT_TOOLTIP_HOME                   1013
#define IDS_FT_TOOLTIP_SEND                   1014
#define IDS_FT_TOOLTIP_RECIEVE                1015
#define IDS_FT_HOME_FOLDER                    1016
#define IDS_FT_DESKTOP_FOLDER                 1017
#define IDS_FT_SIZE_TBYTES                    1018
#define IDS_FT_SIZE_GBYTES                    1019
#define IDS_FT_SIZE_MBYTES                    1020
#define IDS_FT_SIZE_KBYTES                    1021
#define IDS_FT_SIZE_BYTES                     1022
#define IDS_FT_NEW_FOLDER                     1023
#define IDS_FT_COMPUTER                       1024
#define IDS_FT_DRIVE_DESC_DESKTOP             1025
#define IDS_FT_DRIVE_DESC_HOME                1026
#define IDS_FT_DRIVE_DESC_CDROM               1027
#define IDS_FT_DRIVE_DESC_FIXED               1028
#define IDS_FT_DRIVE_DESC_REMOVABLE           1029
#define IDS_FT_DRIVE_DESC_REMOTE              1030
#define IDS_FT_DRIVE_DESC_RAM                 1031
#define IDS_FT_DRIVE_DESC_UNKNOWN             1032
#define IDS_FT_DELETE_CONFORM_FILE            1033
#define IDS_FT_DELETE_CONFORM_DIR             1034
#define IDS_FT_DELETE_CONFORM_MULTI           1035
#define IDS_FT_OP_SESSION_START               1036
#define IDS_FT_OP_SESSION_END                 1037
#define IDS_FT_OP_BROWSE_FOLDERS              1038
#define IDS_FT_OP_RENAME                      1039
#define IDS_FT_OP_REMOVE                      1040
#define IDS_FT_OP_CREATE_FOLDER               1041
#define IDS_FT_OP_BROWSE_DRIVES               1042
#define IDS_FT_OP_SEND_FILE                   1043
#define IDS_FT_OP_RECIEVE_FILE                1044
#define IDS_FT_SEND                           1045
#define IDS_FT_RECIEVE                        1046
#define IDS_FT_SELECTED_OBJECT_COUNT          1047
#define IDS_FT_BROWSE_FOLDERS_ERROR           1048
#define IDS_FT_OP_RENAME_ERROR                1049
#define IDS_FT_OP_REMOVE_ERROR                1050
#define IDS_FT_OP_CREATE_FOLDER_ERROR         1051
#define IDS_FT_OP_BROWSE_DRIVES_ERROR         1052
#define IDS_FT_OP_SEND_FILE_ERROR             1053
#define IDS_FT_OP_RECIEVE_FILE_ERROR          1054

// Status
#define IDS_STATUS_MIN                        10000
#define IDS_STATUS_SUCCESS                    10000
#define IDS_STATUS_NO_CONSOLE_SESSION         10001
#define IDS_STATUS_ACCESS_DENIED              10002
#define IDS_STATUS_FILE_NOT_FOUND             10003
#define IDS_STATUS_PATH_NOT_FOUND             10004
#define IDS_STATUS_FILE_ALREADY_EXISTS        10005
#define IDS_STATUS_PATH_ALREADY_EXISTS        10006
#define IDS_STATUS_DISK_FULL                  10007
#define IDS_STATUS_INVALID_FILE_NAME          10008
#define IDS_STATUS_INVALID_PATH_NAME          10009
#define IDS_STATUS_INVALID_ADDRESS            10010
#define IDS_STATUS_INVALID_PORT               10011
#define IDS_STATUS_CONNECT_TIMEOUT            10012
#define IDS_STATUS_CONNECT_ERROR              10013
#define IDS_STATUS_CONNECTING                 10014
#define IDS_STATUS_NO_DRIVES_FOUND            10015
#define IDS_STATUS_MAX                        10015
#define IDS_STATUS_UNKNOWN                    10016

#endif // _ASPIA_UI__RESOURCE_H
