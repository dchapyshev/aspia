//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/resource.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__RESOURCE_H
#define _ASPIA_UI__RESOURCE_H

#include <windows.h>

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
#define ID_HELP                               512
#define ID_EDIT                               513
#define ID_DELETE                             514
#define ID_ADD                                515
#define ID_REFRESH                            516
#define ID_FOLDER_ADD                         517
#define ID_FOLDER_UP                          518
#define ID_HOME                               519
#define ID_SEND                               520

#define ID_KEY_CTRL_ESC                       600
#define ID_KEY_ALT_TAB                        601
#define ID_KEY_ALT_SHIFT_TAB                  602
#define ID_KEY_PRINTSCREEN                    603
#define ID_KEY_ALT_PRINTSCREEN                604
#define ID_KEY_CTRL_ALT_F12                   605
#define ID_KEY_F12                            606
#define ID_KEY_CTRL_F12                       607

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
#define IDS_APPLICATION_NAME                  100
#define IDS_START                             101
#define IDS_STOP                              102
#define IDS_DONATE_LINK                       103
#define IDS_SITE_LINK                         104
#define IDS_HELP_LINK                         105
#define IDS_ABOUT_STRING                      106
#define IDS_POWER                             107
#define IDS_AUTO_SIZE                         108
#define IDS_SETTINGS                          109
#define IDS_CAD                               110
#define IDS_EXIT                              111
#define IDS_FULLSCREEN                        112
#define IDS_SHORTCUTS                         113
#define IDS_ABOUT                             114
#define IDS_CONNECTION                        115
#define IDS_CONFIRMATION                      116
#define IDS_COMPRESSION_RATIO_FORMAT          117
#define IDS_32BIT                             118
#define IDS_24BIT                             119
#define IDS_16BIT                             120
#define IDS_15BIT                             121
#define IDS_12BIT                             122
#define IDS_8BIT                              123
#define IDS_6BIT                              124
#define IDS_3BIT                              125
#define IDS_INFORMATION                       126
#define IDS_INVALID_USERNAME                  127
#define IDS_USER_ALREADY_EXISTS               128
#define IDS_INVALID_PASSWORD                  129
#define IDS_PASSWORDS_NOT_MATCH               130
#define IDS_SESSION_TYPE_DESKTOP_MANAGE       131
#define IDS_SESSION_TYPE_DESKTOP_VIEW         132
#define IDS_SESSION_TYPE_POWER_MANAGE         133
#define IDS_SESSION_TYPE_FILE_TRANSFER        134
#define IDS_SESSION_TYPE_SYSTEM_INFO          135
#define IDS_DELETE_USER_CONFORMATION          136
#define IDS_USER_LIST_MODIFIED                137

// File transfer
#define IDS_FT_LOCAL_COMPUTER                 1000
#define IDS_FT_REMOTE_COMPUTER                1001
#define IDS_FT_COLUMN_NAME                    1002
#define IDS_FT_COLUMN_SIZE                    1003
#define IDS_FT_COLUMN_TYPE                    1004
#define IDS_FT_TOOLTIP_REFRESH                1005
#define IDS_FT_TOOLTIP_DELETE                 1006
#define IDS_FT_TOOLTIP_FOLDER_ADD             1007
#define IDS_FT_TOOLTIP_FOLDER_UP              1008
#define IDS_FT_TOOLTIP_HOME                   1009
#define IDS_FT_TOOLTIP_SEND                   1010
#define IDS_FT_TOOLTIP_RECIEVE                1011

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
#define IDS_STATUS_INVALID_ADDRESS            10008
#define IDS_STATUS_INVALID_PORT               10009
#define IDS_STATUS_CONNECT_TIMEOUT            10010
#define IDS_STATUS_CONNECT_ERROR              10011
#define IDS_STATUS_CONNECTING                 10012
#define IDS_STATUS_MAX                        10012
#define IDS_STATUS_UNKNOWN                    10013

#endif // _ASPIA_UI__RESOURCE_H
