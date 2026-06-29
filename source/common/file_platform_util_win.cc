//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include <QImage>

#include "base/win/scoped_co_mem.h"
#include "base/win/scoped_user_object.h"

#include <shellapi.h>
#include <shlobj.h>

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
        ScopedHICON icon(icon_info.hIcon);
        if (icon.isValid())
            return QPixmap::fromImage(QImage::fromHICON(icon));
    }

    return QIcon(QStringLiteral(":/img/file.svg"));
}

//--------------------------------------------------------------------------------------------------
// Windows has no stock icon id for the Downloads/Documents/Pictures folders (only the generic
// SIID_FOLDER), so their distinct shell icons are taken from the known folder itself by its path.
QIcon knownFolderIcon(REFKNOWNFOLDERID folder_id)
{
    ScopedCoMem<wchar_t> folder_path;
    if (FAILED(SHGetKnownFolderPath(folder_id, KF_FLAG_DEFAULT, nullptr, &folder_path)) || !folder_path)
        return stockIcon(SIID_FOLDER);

    SHFILEINFO file_info;
    memset(&file_info, 0, sizeof(file_info));
    SHGetFileInfoW(folder_path, 0, &file_info, sizeof(file_info), SHGFI_ICON | SHGFI_SMALLICON);

    ScopedHICON icon(file_info.hIcon);
    if (icon.isValid())
        return QPixmap::fromImage(QImage::fromHICON(icon));

    return stockIcon(SIID_FOLDER);
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

    ScopedHICON icon(file_info.hIcon);
    if (icon.isValid())
    {
        return std::pair<QIcon, QString>(QPixmap::fromImage(QImage::fromHICON(icon)),
                                         QString::fromWCharArray(file_info.szTypeName));
    }

    return std::pair<QIcon, QString>(QIcon(QStringLiteral(":/img/file.svg")),
                                     QString::fromWCharArray(file_info.szTypeName));
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
    static const SHSTOCKICONID SIID_DESKTOP = static_cast<SHSTOCKICONID>(34); // NOLINT

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

        case proto::file_transfer::DriveList::Item::TYPE_DOWNLOAD_FOLDER:
            return knownFolderIcon(FOLDERID_Downloads);

        case proto::file_transfer::DriveList::Item::TYPE_PICTURES_FOLDER:
            return knownFolderIcon(FOLDERID_Pictures);

        case proto::file_transfer::DriveList::Item::TYPE_DOCUMENTS_FOLDER:
            return knownFolderIcon(FOLDERID_Documents);

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

    if (file_name == ".." || file_name == ".")
        return false;

    return true;
}
