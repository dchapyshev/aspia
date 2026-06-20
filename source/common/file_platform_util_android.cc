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

#include <QMimeDatabase>

namespace {

// The file name can not be shorter than 1 character.
const int kMinFileNameLength = 1;

// FAT limits file and folder names to 255 characters.
const int kMaxFileNameLength = 255;

// A path is at least one character long ("/").
const int kMinPathLength = 1;

} // namespace

//--------------------------------------------------------------------------------------------------
// static
std::pair<QIcon, QString> FilePlatformUtil::fileTypeInfo(const QString& file_name)
{
    static QMimeDatabase mime_database;
    QMimeType mime_type = mime_database.mimeTypeForFile(file_name, QMimeDatabase::MatchExtension);
    return std::pair<QIcon, QString>(QIcon(":/img/file.svg"), mime_type.comment());
}

//--------------------------------------------------------------------------------------------------
// static
QIcon FilePlatformUtil::computerIcon()
{
    return QIcon(":/img/computer.svg");
}

//--------------------------------------------------------------------------------------------------
// static
QIcon FilePlatformUtil::directoryIcon()
{
    return QIcon(":/img/folder.svg");
}

//--------------------------------------------------------------------------------------------------
// static
QIcon FilePlatformUtil::driveIcon(proto::file_transfer::DriveList::Item::Type type)
{
    switch (type)
    {
        case proto::file_transfer::DriveList::Item::TYPE_HOME_FOLDER:
            return QIcon(":/img/home.svg");

        case proto::file_transfer::DriveList::Item::TYPE_DESKTOP_FOLDER:
            return QIcon(":/img/desktop.svg");

        case proto::file_transfer::DriveList::Item::TYPE_ROOT_DIRECTORY:
            return QIcon(":/img/folder.svg");

        case proto::file_transfer::DriveList::Item::TYPE_DOWNLOAD_FOLDER:
            return QIcon(":/img/folder-downloads.svg");

        case proto::file_transfer::DriveList::Item::TYPE_PICTURES_FOLDER:
            return QIcon(":/img/folder-pictures.svg");

        case proto::file_transfer::DriveList::Item::TYPE_DOCUMENTS_FOLDER:
            return QIcon(":/img/folder-documents.svg");

        case proto::file_transfer::DriveList::Item::TYPE_CAMERA_FOLDER:
            return QIcon(":/img/folder-camera.svg");

        case proto::file_transfer::DriveList::Item::TYPE_SD_CARD:
            return QIcon(":/img/sd.svg");

        default:
            return QIcon(":/img/hdd.svg");
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
    if (path.length() < kMinPathLength)
        return false;

    const QList<QChar>& invalid_characters = invalidPathCharacters();

    for (const QChar& character : path)
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
    const int length = file_name.length();
    if (length < kMinFileNameLength || length > kMaxFileNameLength)
        return false;

    const QList<QChar>& invalid_characters = invalidFileNameCharacters();

    for (const QChar& character : file_name)
    {
        if (invalid_characters.contains(character))
            return false;
    }

    if (file_name == QLatin1String("..") || file_name == QLatin1String("."))
        return false;

    return true;
}
