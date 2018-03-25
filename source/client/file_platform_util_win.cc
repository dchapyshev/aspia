//
// PROJECT:         Aspia
// FILE:            client/file_platform_util_win.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/file_platform_util.h"

#if !defined(Q_OS_WIN)
#error This file is only for MS Windows
#endif

#include <QtWin>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>

namespace aspia {

namespace {

QIcon stockIcon(SHSTOCKICONID icon_id)
{
    SHSTOCKICONINFO icon_info;

    memset(&icon_info, 0, sizeof(icon_info));
    icon_info.cbSize = sizeof(icon_info);

    if (FAILED(SHGetStockIconInfo(icon_id, SHGSI_ICON | SHGSI_SMALLICON, &icon_info)))
        return QIcon();

    return QtWin::fromHICON(icon_info.hIcon);
}

} // namespace

// static
QPair<QIcon, QString> FilePlatformUtil::fileTypeInfo(const QString& file_name)
{
    SHFILEINFO file_info;
    memset(&file_info, 0, sizeof(file_info));

    SHGetFileInfoW(file_name.toStdWString().c_str(),
                   FILE_ATTRIBUTE_NORMAL,
                   &file_info,
                   sizeof(file_info),
                   SHGFI_USEFILEATTRIBUTES | SHGFI_ICON | SHGFI_SMALLICON | SHGFI_TYPENAME);

    return QPair<QIcon, QString>(QtWin::fromHICON(file_info.hIcon),
                                 QString::fromWCharArray(file_info.szTypeName));
}

// static
QIcon FilePlatformUtil::computerIcon()
{
    return stockIcon(SIID_DESKTOPPC);
}

// static
QIcon FilePlatformUtil::directoryIcon()
{
    return stockIcon(SIID_FOLDER);
}

// static
QIcon FilePlatformUtil::driveIcon(proto::file_transfer::DriveList::Item::Type type)
{
    // Desktop (not present in official header)
    static const SHSTOCKICONID SIID_DESKTOP = static_cast<SHSTOCKICONID>(34);

    switch (type)
    {
        case proto::file_transfer::DriveList::Item::TYPE_FIXED:
            return stockIcon(SIID_DRIVEFIXED);

        case proto::file_transfer::DriveList::Item::TYPE_CDROM:
            return stockIcon(SIID_DRIVECD);

        case proto::file_transfer::DriveList::Item::TYPE_REMOVABLE:
            return stockIcon(SIID_DRIVEREMOVE);

        case proto::file_transfer::DriveList::Item::TYPE_REMOTE:
            return stockIcon(SIID_DRIVENET);

        case proto::file_transfer::DriveList::Item::TYPE_RAM:
            return stockIcon(SIID_DRIVERAM);

        case proto::file_transfer::DriveList::Item::TYPE_HOME_FOLDER:
            return stockIcon(SIID_FOLDER);

        case proto::file_transfer::DriveList::Item::TYPE_DESKTOP_FOLDER:
            return stockIcon(SIID_DESKTOP);

        default:
            return QIcon();
    }
}

// static
proto::file_transfer::DriveList::Item::Type FilePlatformUtil::driveType(const QString& drive_path)
{
    switch (GetDriveTypeW(drive_path.toStdWString().c_str()))
    {
        case DRIVE_FIXED:
            return proto::file_transfer::DriveList::Item::TYPE_FIXED;

        case DRIVE_REMOVABLE:
            return proto::file_transfer::DriveList::Item::TYPE_REMOVABLE;

        case DRIVE_CDROM:
            return proto::file_transfer::DriveList::Item::TYPE_CDROM;

        case DRIVE_REMOTE:
            return proto::file_transfer::DriveList::Item::TYPE_REMOTE;

        case DRIVE_RAMDISK:
            return proto::file_transfer::DriveList::Item::TYPE_RAM;

        default:
            return proto::file_transfer::DriveList::Item::TYPE_UNKNOWN;
    }
}

} // namespace aspia
