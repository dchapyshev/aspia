//
// PROJECT:         Aspia Remote Desktop
// FILE:            gui/resource.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_GUI__RESOURCE_H
#define _ASPIA_GUI__RESOURCE_H

#include <windows.h>
#include <atlres.h>

//
// Icons
//
#define IDI_MAINICON                       100
#define IDI_ABOUT                          101
#define IDI_CAD                            102
#define IDI_EXIT                           103
#define IDI_FULLSCREEN                     104
#define IDI_IDEALSIZE                      105
#define IDI_KEYS                           106
#define IDI_POWER                          107
#define IDI_SETTINGS                       108

//
// Dialogs
//
#define IDD_MAIN                           100
#define IDD_AUTH                           101
#define IDD_ABOUT                          102
#define IDD_STATUS                         103
#define IDD_SETTINGS                       104
#define IDD_VIEWER                         105
#define IDD_USERS                          106
#define IDD_USER_ADD                       107

//
// Controls
//
#define IDC_SERVER_ADDRESS_EDIT            100
#define IDC_SERVER_PORT_EDIT               101
#define IDC_SERVER_DEFAULT_PORT_CHECK      102
#define IDC_CONNECT_BUTTON                 103
#define IDC_IP_LIST                        104
#define IDC_START_SERVER_BUTTON            105
#define IDC_SERVER_STATUS_TEXT             106
#define IDC_CLIENT_COUNT_TEXT              107
#define IDC_ABOUT_ICON                     108
#define IDC_VERSION_TEXT                   109
#define IDC_ABOUT_EDIT                     110
#define IDC_DONATE_BUTTON                  111
#define IDC_SITE_BUTTON                    112
#define IDC_STATUS_EDIT                    113
#define IDC_USERNAME_EDIT                  114
#define IDC_PASSWORD_EDIT                  115
#define IDC_PASSWORD2_EDIT                 116
#define IDC_CODEC_COMBO                    117
#define IDC_COLOR_DEPTH_COMBO              118
#define IDC_COMPRESS_RATIO_TEXT            119
#define IDC_COMPRESS_RATIO_TRACKBAR        120
#define IDC_ENABLE_DESKTOP_EFFECTS_CHECK   121
#define IDC_ENABLE_CURSOR_SHAPE_CHECK      122
#define IDC_INTERVAL_EDIT                  123
#define IDC_INTERVAL_UPDOWN                124
#define IDC_ENABLE_CLIPBOARD_CHECK         125
#define IDC_FAST_TEXT                      126
#define IDC_BEST_TEXT                      127
#define IDC_COLOR_DEPTH_TEXT               128
#define IDC_USER_LIST                      129
#define IDÑ_ADD_USER                       130
#define IDC_EDIT_USER                      131
#define IDC_DELETE_USER                    132
#define IDC_DISABLE_USER_CHECK             133
#define IDC_USER_FEATURES_LIST             134

//
// Commands
//
#define ID_POWER                           500
#define ID_CAD                             501
#define ID_SHORTCUTS                       502
#define ID_AUTO_SIZE                       503
#define ID_FULLSCREEN                      504
#define ID_SETTINGS                        505
#define ID_SHOWHIDE                        506
#define ID_INSTALL_SERVICE                 507
#define ID_REMOVE_SERVICE                  508
#define ID_USERS                           509

#define ID_KEY_FIRST                       513
#define ID_KEY_CTRL_ESC                    513
#define ID_KEY_ALT_TAB                     514
#define ID_KEY_ALT_SHIFT_TAB               515
#define ID_KEY_PRINTSCREEN                 516
#define ID_KEY_ALT_PRINTSCREEN             517
#define ID_KEY_CTRL_ALT_F12                518
#define ID_KEY_F12                         519
#define ID_KEY_CTRL_F12                    520
#define ID_KEY_LAST                        520

#define ID_POWER_FIRST                     521
#define ID_POWER_SHUTDOWN                  521
#define ID_POWER_REBOOT                    522
#define ID_POWER_HIBERNATE                 523
#define ID_POWER_SUSPEND                   524
#define ID_POWER_LOGOFF                    525
#define ID_POWER_LAST                      525

//
// Menu
//
#define IDR_POWER                          100
#define IDR_SHORTCUTS                      101
#define IDR_TRAY                           102
#define IDR_MAIN                           103

//
// Strings
//
#define IDS_APPLICATION_NAME               100
#define IDS_SERVER_STARTED                 101
#define IDS_SERVER_STOPPED                 102
#define IDS_START                          103
#define IDS_STOP                           104
#define IDS_NO_CLIENTS                     105
#define IDS_CLIENT_COUNT_FORMAT            106
#define IDS_DONATE_LINK                    107
#define IDS_SITE_LINK                      108
#define IDS_HELP_LINK                      109
#define IDS_ABOUT_STRING                   110
#define IDS_POWER                          111
#define IDS_AUTO_SIZE                      112
#define IDS_SETTINGS                       113
#define IDS_CAD                            114
#define IDS_EXIT                           115
#define IDS_FULLSCREEN                     116
#define IDS_SHORTCUTS                      117
#define IDS_ABOUT                          118
#define IDS_STATUS_CONNECTING_FORMAT       119
#define IDS_STATUS_CONNECTED               120
#define IDS_STATUS_NOT_CONNECTED           121
#define IDS_STATUS_ACCESS_DENIED           122
#define IDS_STATUS_KEY_EXCHANGE_ERROR      123
#define IDS_CONFIRMATION                   124
#define IDS_CONF_POWER_SHUTDOWN            125
#define IDS_CONF_POWER_REBOOT              126
#define IDS_CONF_POWER_HIBERNATE           127
#define IDS_CONF_POWER_SUSPEND             128
#define IDS_CONF_POWER_LOGOFF              129
#define IDS_COMPRESSION_RATIO_FORMAT       130
#define IDS_32BIT                          131
#define IDS_24BIT                          132
#define IDS_16BIT                          133
#define IDS_15BIT                          134
#define IDS_12BIT                          135
#define IDS_8BIT                           136
#define IDS_6BIT                           137
#define IDS_3BIT                           138
#define IDS_INFORMATION                    139

#endif // _ASPIA_GUI__RESOURCE_H
