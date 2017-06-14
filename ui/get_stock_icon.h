//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/get_stock_icon.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__GET_STOCK_ICON_H
#define _ASPIA_UI__GET_STOCK_ICON_H

#include <shellapi.h>

//
// Windows XP does not support function SHGetStockIconInfo.
// This file presents a simplified implementation of this function.
//

#if (NTDDI_VERSION < NTDDI_VISTA)

typedef enum SHSTOCKICONID
{
    SIID_DOCNOASSOC          = 0,     // Document (blank page), no associated program
    SIID_DOCASSOC            = 1,     // Document with an associated program
    SIID_APPLICATION         = 2,     // Generic application with no custom icon
    SIID_FOLDER              = 3,     // Folder (closed)
    SIID_FOLDEROPEN          = 4,     // Folder (open)
    SIID_DRIVE525            = 5,     // 5.25" floppy disk drive
    SIID_DRIVE35             = 6,     // 3.5" floppy disk drive
    SIID_DRIVEREMOVE         = 7,     // Removable drive
    SIID_DRIVEFIXED          = 8,     // Fixed (hard disk) drive
    SIID_DRIVENET            = 9,     // Network drive
    SIID_DRIVENETDISABLED    = 10,    // Disconnected network drive
    SIID_DRIVECD             = 11,    // CD drive
    SIID_DRIVERAM            = 12,    // RAM disk drive
    SIID_WORLD               = 13,    // Entire network
    SIID_SERVER              = 15,    // A computer on the network
    SIID_PRINTER             = 16,    // Printer
    SIID_MYNETWORK           = 17,    // My network places
    SIID_FIND                = 22,    // Find
    SIID_HELP                = 23,    // Help
    SIID_SHARE               = 28,    // Overlay for shared items
    SIID_LINK                = 29,    // Overlay for shortcuts to items
    SIID_SLOWFILE            = 30,    // Overlay for slow items
    SIID_RECYCLER            = 31,    // Empty recycle bin
    SIID_RECYCLERFULL        = 32,    // Full recycle bin
    SIID_DESKTOP             = 34,    // Desktop (not present in official header)
    SIID_MEDIACDAUDIO        = 40,    // Audio CD Media
    SIID_LOCK                = 47,    // Security lock
    SIID_AUTOLIST            = 49,    // AutoList
    SIID_PRINTERNET          = 50,    // Network printer
    SIID_SERVERSHARE         = 51,    // Server share
    SIID_PRINTERFAX          = 52,    // Fax printer
    SIID_PRINTERFAXNET       = 53,    // Networked Fax Printer
    SIID_PRINTERFILE         = 54,    // Print to File
    SIID_STACK               = 55,    // Stack
    SIID_MEDIASVCD           = 56,    // SVCD Media
    SIID_STUFFEDFOLDER       = 57,    // Folder containing other items
    SIID_DRIVEUNKNOWN        = 58,    // Unknown drive
    SIID_DRIVEDVD            = 59,    // DVD Drive
    SIID_MEDIADVD            = 60,    // DVD Media
    SIID_MEDIADVDRAM         = 61,    // DVD-RAM Media
    SIID_MEDIADVDRW          = 62,    // DVD-RW Media
    SIID_MEDIADVDR           = 63,    // DVD-R Media
    SIID_MEDIADVDROM         = 64,    // DVD-ROM Media
    SIID_MEDIACDAUDIOPLUS    = 65,    // CD+ (Enhanced CD) Media
    SIID_MEDIACDRW           = 66,    // CD-RW Media
    SIID_MEDIACDR            = 67,    // CD-R Media
    SIID_MEDIACDBURN         = 68,    // Burning CD
    SIID_MEDIABLANKCD        = 69,    // Blank CD Media
    SIID_MEDIACDROM          = 70,    // CD-ROM Media
    SIID_AUDIOFILES          = 71,    // Audio files
    SIID_IMAGEFILES          = 72,    // Image files
    SIID_VIDEOFILES          = 73,    // Video files
    SIID_MIXEDFILES          = 74,    // Mixed files
    SIID_FOLDERBACK          = 75,    // Folder back
    SIID_FOLDERFRONT         = 76,    // Folder front
    SIID_SHIELD              = 77,    // Security shield. Use for UAC prompts only.
    SIID_WARNING             = 78,    // Warning
    SIID_INFO                = 79,    // Informational
    SIID_ERROR               = 80,    // Error
    SIID_KEY                 = 81,    // Key / Secure
    SIID_SOFTWARE            = 82,    // Software
    SIID_RENAME              = 83,    // Rename
    SIID_DELETE              = 84,    // Delete
    SIID_MEDIAAUDIODVD       = 85,    // Audio DVD Media
    SIID_MEDIAMOVIEDVD       = 86,    // Movie DVD Media
    SIID_MEDIAENHANCEDCD     = 87,    // Enhanced CD Media
    SIID_MEDIAENHANCEDDVD    = 88,    // Enhanced DVD Media
    SIID_MEDIAHDDVD          = 89,    // HD-DVD Media
    SIID_MEDIABLURAY         = 90,    // BluRay Media
    SIID_MEDIAVCD            = 91,    // VCD Media
    SIID_MEDIADVDPLUSR       = 92,    // DVD+R Media
    SIID_MEDIADVDPLUSRW      = 93,    // DVD+RW Media
    SIID_DESKTOPPC           = 94,    // Desktop computer
    SIID_MOBILEPC            = 95,    // Mobile computer (laptop/notebook)
    SIID_USERS               = 96,    // Users
    SIID_MEDIASMARTMEDIA     = 97,    // Smart Media
    SIID_MEDIACOMPACTFLASH   = 98,    // Compact Flash
    SIID_DEVICECELLPHONE     = 99,    // Cell phone
    SIID_DEVICECAMERA        = 100,   // Camera
    SIID_DEVICEVIDEOCAMERA   = 101,   // Video camera
    SIID_DEVICEAUDIOPLAYER   = 102,   // Audio player
    SIID_NETWORKCONNECT      = 103,   // Connect to network
    SIID_INTERNET            = 104,   // Internet
    SIID_ZIPFILE             = 105,   // ZIP file
    SIID_SETTINGS            = 106,   // Settings
    // 107-131 are internal Vista RTM icons
    // 132-159 for SP1 icons
    SIID_DRIVEHDDVD          = 132,   // HDDVD Drive (all types)
    SIID_DRIVEBD             = 133,   // BluRay Drive (all types)
    SIID_MEDIAHDDVDROM       = 134,   // HDDVD-ROM Media
    SIID_MEDIAHDDVDR         = 135,   // HDDVD-R Media
    SIID_MEDIAHDDVDRAM       = 136,   // HDDVD-RAM Media
    SIID_MEDIABDROM          = 137,   // BluRay ROM Media
    SIID_MEDIABDR            = 138,   // BluRay R Media
    SIID_MEDIABDRE           = 139,   // BluRay RE Media (Rewriable and RAM)
    SIID_CLUSTEREDDRIVE      = 140,   // Clustered disk
    // 160+ are for Windows 7 icons
    SIID_MAX_ICONS           = 174,
} SHSTOCKICONID;

typedef struct _SHSTOCKICONINFO
{
    DWORD cbSize;
    HICON hIcon;
    int   iSysImageIndex;
    int   iIcon;
    WCHAR szPath[MAX_PATH];
} SHSTOCKICONINFO;

#define SHGSI_ICONLOCATION      0 // you always get the icon location
#define SHGSI_ICON              SHGFI_ICON
#define SHGSI_SYSICONINDEX      SHGFI_SYSICONINDEX
#define SHGSI_LINKOVERLAY       SHGFI_LINKOVERLAY
#define SHGSI_SELECTED          SHGFI_SELECTED
#define SHGSI_LARGEICON         SHGFI_LARGEICON
#define SHGSI_SMALLICON         SHGFI_SMALLICON
#define SHGSI_SHELLICONSIZE     SHGFI_SHELLICONSIZE

// Supported flags: SHGSI_ICONLOCATION, SHGSI_ICON, SHGSI_LARGEICON,
//                  SHGSI_SMALLICON, SHGSI_SYSICONINDEX.
// Not supported flags: SHGSI_LINKOVERLAY, SHGSI_SELECTED, SHGSI_SHELLICONSIZE.
HRESULT WINAPI SHGetStockIconInfo(SHSTOCKICONID id, UINT flags, SHSTOCKICONINFO* sii);

#endif // (NTDDI_VERSION < NTDDI_VISTA)

#endif // _ASPIA_UI__GET_STOCK_ICON_H
