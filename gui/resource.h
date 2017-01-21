//
// PROJECT:         Aspia Remote Desktop
// FILE:            gui/resource.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_GUI__RESOURCE_H
#define _ASPIA_GUI__RESOURCE_H

#include <windows.h>

//
// Icons
//
#define IDI_MAINICON                100
#define IDI_ABOUT                   101
#define IDI_BELL                    102
#define IDI_CAD                     103
#define IDI_CLIP_RECV               104
#define IDI_CLIP_SEND               105
#define IDI_EXIT                    106
#define IDI_FULLSCREEN              107
#define IDI_IDEALSIZE               108
#define IDI_KEYS                    109
#define IDI_POWER                   110
#define IDI_SETTINGS                111

//
// Dialogs
//
#define IDD_MAIN                    100
#define IDD_AUTH                    101
#define IDD_ABOUT                   102
#define IDD_STATUS                  103
#define IDD_SETTINGS                104

//
// Controls
//
#define IDC_SERVER_ADDRESS_EDIT        100
#define IDC_SERVER_PORT_EDIT           101
#define IDC_SERVER_DEFAULT_PORT_CHECK  102
#define IDC_CONNECT_BUTTON             103
#define IDC_IP_LIST                    104
#define IDC_START_SERVER_BUTTON        105
#define IDC_SERVER_STATUS_TEXT         106
#define IDC_CLIENT_COUNT_TEXT          107
#define IDC_ABOUT_ICON                 108
#define IDC_VERSION_TEXT               109
#define IDC_ABOUT_EDIT                 110
#define IDC_DONATE_BUTTON              111
#define IDC_SITE_BUTTON                112
#define IDC_STATUS_EDIT                113
#define IDC_USERNAME_EDIT              114
#define IDC_PASSWORD_EDIT              115
#define IDC_CODEC_COMBO                116
#define IDC_COLOR_DEPTH_COMBO          117
#define IDC_COMPRESS_RATIO_TEXT        118
#define IDC_COMPRESS_RATIO_TRACKBAR    119
#define IDC_DESKTOP_EFFECTS_CHECK      120
#define IDC_REMOTE_CURSOR_CHECK        121
#define IDC_INTERVAL_EDIT              122
#define IDC_INTERVAL_UPDOWN            123
#define IDC_AUTOSEND_CLIPBOARD_CHECK   124
#define IDC_FAST_TEXT                  125
#define IDC_BEST_TEXT                  126
#define IDC_COLOR_DEPTH_TEXT           127

//
// Commands
//
#define ID_POWER                    100
#define ID_CAD                      101
#define ID_SHORTCUTS                102
#define ID_BELL                     103
#define ID_CLIP_RECV                104
#define ID_CLIP_SEND                105
#define ID_AUTO_SIZE                106
#define ID_FULLSCREEN               107
#define ID_ABOUT                    108
#define ID_EXIT                     109
#define ID_SETTINGS                 110
#define ID_SHOWHIDE                 112

#define ID_KEY_FIRST                113
#define ID_KEY_CTRL_ESC             113
#define ID_KEY_ALT_TAB              114
#define ID_KEY_ALT_SHIFT_TAB        115
#define ID_KEY_PRINTSCREEN          116
#define ID_KEY_ALT_PRINTSCREEN      117
#define ID_KEY_CTRL_ALT_F12         118
#define ID_KEY_F12                  119
#define ID_KEY_CTRL_F12             120
#define ID_KEY_LAST                 120

#define ID_POWER_FIRST              121
#define ID_POWER_SHUTDOWN           121
#define ID_POWER_REBOOT             122
#define ID_POWER_HIBERNATE          123
#define ID_POWER_SUSPEND            124
#define ID_POWER_LOGOFF             125
#define ID_POWER_LAST               125

//
// Menu
//
#define IDR_POWER                   100
#define IDR_SHORTCUTS               101
#define IDR_TRAY                    102

//
// Strings
//
#define IDS_APPLICATION_NAME           100
#define IDS_SERVER_STARTED             101
#define IDS_SERVER_STOPPED             102
#define IDS_START                      103
#define IDS_STOP                       104
#define IDS_NO_CLIENTS                 105
#define IDS_CLIENT_COUNT_FORMAT        106
#define IDS_DONATE_LINK                107
#define IDS_SITE_LINK                  108
#define IDS_ABOUT_STRING               109
#define IDS_BELL                       110
#define IDS_POWER                      111
#define IDS_AUTO_SIZE                  112
#define IDS_SETTINGS                   113
#define IDS_CAD                        114
#define IDS_EXIT                       115
#define IDS_FULLSCREEN                 116
#define IDS_SHORTCUTS                  117
#define IDS_CLIP_RECV                  118
#define IDS_CLIP_SEND                  119
#define IDS_ABOUT                      120
#define IDS_STATUS_CONNECTING_FORMAT   121
#define IDS_STATUS_CONNECTED           122
#define IDS_CONFIRMATION               123
#define IDS_CONF_POWER_SHUTDOWN        124
#define IDS_CONF_POWER_REBOOT          125
#define IDS_CONF_POWER_HIBERNATE       126
#define IDS_CONF_POWER_SUSPEND         127
#define IDS_CONF_POWER_LOGOFF          128
#define IDS_COMPRESSION_RATIO_FORMAT   129
#define IDS_32BIT                      130
#define IDS_24BIT                      131
#define IDS_16BIT                      132
#define IDS_15BIT                      133
#define IDS_12BIT                      134
#define IDS_8BIT                       135
#define IDS_6BIT                       136
#define IDS_3BIT                       137

#endif // _ASPIA_GUI__RESOURCE_H
