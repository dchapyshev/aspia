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

#include <QMimeDatabase>

#include "base/logging.h"

namespace common {

namespace {

// Minimal path name is 2 characters (for example: "/").
const int kMinPathLength = 1;

// Under Window the length of a full path is 260 characters.
const int kMaxPathLength = 260;

// The file name can not be shorter than 1 character.
const int kMinFileNameLength = 1;

// For FAT: file and folder names may be up to 255 characters long.
// For NTFS: file and folder names may be up to 256 characters long.
// We use FAT variant: 255 characters long.
const int kMaxFileNameLength = (kMaxPathLength - 5);

} // namespace

//--------------------------------------------------------------------------------------------------
// static
std::pair<QIcon, QString> FilePlatformUtil::fileTypeInfo(const QString& file_name)
{
    static QMimeDatabase mime_database;
    QMimeType mime_type = mime_database.mimeTypeForFile(file_name, QMimeDatabase::MatchExtension);
    return std::pair<QIcon, QString>(QIcon(QStringLiteral(":/img/file.svg")), mime_type.comment());
}

//--------------------------------------------------------------------------------------------------
// static
QIcon FilePlatformUtil::computerIcon()
{
    return QIcon(QStringLiteral(":/img/computer.svg"));
}

//--------------------------------------------------------------------------------------------------
// static
QIcon FilePlatformUtil::directoryIcon()
{
    return QIcon(QStringLiteral(":/img/folder.svg"));
}

//--------------------------------------------------------------------------------------------------
// static
QIcon FilePlatformUtil::driveIcon(proto::DriveList::Item::Type type)
{
    QString icon_name;

    switch (type)
    {
        case proto::DriveList::Item::TYPE_HOME_FOLDER:
            icon_name = QStringLiteral(":/img/home.svg");
            break;

        case proto::DriveList::Item::TYPE_DESKTOP_FOLDER:
            icon_name = QStringLiteral(":/img/desktop.svg");
            break;

        case proto::DriveList::Item::TYPE_ROOT_DIRECTORY:
            icon_name = QStringLiteral(":/img/folder.svg");
            break;

        case proto::DriveList::Item::TYPE_REMOTE:
        case proto::DriveList::Item::TYPE_CDROM:
        case proto::DriveList::Item::TYPE_RAM:
        case proto::DriveList::Item::TYPE_FIXED:
        default:
            icon_name = QStringLiteral(":/img/hdd.svg");
            break;
    }

    return QIcon(icon_name);
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
