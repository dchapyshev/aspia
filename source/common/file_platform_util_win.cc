//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#include "common/file_platform_util.h"

#include "base/win/scoped_user_object.h"

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <QtWin>
#else
#include <QImage>
#endif

#include <shellapi.h>

namespace common {

namespace {

// Minimal path name is 1 characters.
const int kMinPathLength = 1;

// Under Window the length of a full path is 260 characters.
const int kMaxPathLength = MAX_PATH;

// The file name can not be shorter than 1 character.
const int kMinFileNameLength = 1;

// For FAT: file and folder names may be up to 255 characters long.
// For NTFS: file and folder names may be up to 256 characters long.
// We use FAT variant: 255 characters long.
const int kMaxFileNameLength = (MAX_PATH - 5);

//--------------------------------------------------------------------------------------------------
QIcon stockIcon(SHSTOCKICONID icon_id)
{
    SHSTOCKICONINFO icon_info;

    memset(&icon_info, 0, sizeof(icon_info));
    icon_info.cbSize = sizeof(icon_info);

    if (SUCCEEDED(SHGetStockIconInfo(icon_id, SHGSI_ICON | SHGSI_SMALLICON, &icon_info)))
    {
        base::ScopedHICON icon(icon_info.hIcon);
        if (icon.isValid())
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
            return QtWin::fromHICON(icon);
#else
            return QPixmap::fromImage(QImage::fromHICON(icon));
#endif
    }

    return QIcon(QStringLiteral(":/img/document.png"));
}

} // namespace

//--------------------------------------------------------------------------------------------------
// static
std::pair<QIcon, QString> FilePlatformUtil::fileTypeInfo(const QString& file_name)
{
    SHFILEINFO file_info;
    memset(&file_info, 0, sizeof(file_info));

    SHGetFileInfoW(qUtf16Printable(file_name),
                   FILE_ATTRIBUTE_NORMAL,
                   &file_info,
                   sizeof(file_info),
                   SHGFI_USEFILEATTRIBUTES | SHGFI_ICON | SHGFI_SMALLICON | SHGFI_TYPENAME);

    base::ScopedHICON icon(file_info.hIcon);
    if (icon.isValid())
    {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        return std::pair<QIcon, QString>(QtWin::fromHICON(icon),
#else
        return std::pair<QIcon, QString>(QPixmap::fromImage(QImage::fromHICON(icon)),
#endif
                                         QString::fromUtf16(
                                             reinterpret_cast<const ushort*>(file_info.szTypeName)));
    }

    return std::pair<QIcon, QString>(QIcon(QStringLiteral(":/img/document.png")),
                                     QString::fromUtf16(
                                         reinterpret_cast<const ushort*>(file_info.szTypeName)));
}

//--------------------------------------------------------------------------------------------------
// static
QIcon FilePlatformUtil::computerIcon()
{
    return stockIcon(SIID_DESKTOPPC);
}

//--------------------------------------------------------------------------------------------------
// static
QIcon FilePlatformUtil::directoryIcon()
{
    return stockIcon(SIID_FOLDER);
}

//--------------------------------------------------------------------------------------------------
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
            return stockIcon(SIID_DRIVEFIXED);
    }
}

//--------------------------------------------------------------------------------------------------
// static
const QList<QChar>& FilePlatformUtil::invalidFileNameCharacters()
{
    static const QList<QChar> kInvalidCharacters =
        { '/', '\\', ':', '*', '?', '"', '<', '>', '|' };
    return kInvalidCharacters;
}

//--------------------------------------------------------------------------------------------------
// static
const QList<QChar>& FilePlatformUtil::invalidPathCharacters()
{
    static const QList<QChar> kInvalidCharacters = { '*', '?', '"', '<', '>', '|' };
    return kInvalidCharacters;
}

//--------------------------------------------------------------------------------------------------
// static
bool FilePlatformUtil::isValidPath(const QString& path)
{
    int length = path.length();

    if (length < kMinPathLength || length > kMaxPathLength)
        return false;

    const QList<QChar>& invalid_characters = invalidPathCharacters();

    for (const auto& character : path)
    {
        if (invalid_characters.contains(character))
            return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
// static
bool FilePlatformUtil::isValidFileName(const QString& file_name)
{
    int length = file_name.length();

    if (length < kMinFileNameLength || length > kMaxFileNameLength)
        return false;

    const QList<QChar>& invalid_characters = invalidFileNameCharacters();

    for (const auto& character : file_name)
    {
        if (invalid_characters.contains(character))
            return false;
    }

    if (file_name == QLatin1String("..") || file_name == QLatin1String("."))
        return false;

    return true;
}

} // namespace common
