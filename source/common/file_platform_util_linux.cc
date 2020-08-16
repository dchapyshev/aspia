//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/logging.h"

namespace common {

// static
QPair<QIcon, QString> FilePlatformUtil::fileTypeInfo(const QString& file_name)
{
    NOTIMPLEMENTED();
    return QPair<QIcon, QString>(QIcon(), QString());
}

// static
QIcon FilePlatformUtil::computerIcon()
{
    NOTIMPLEMENTED();
    return QIcon();
}

// static
QIcon FilePlatformUtil::directoryIcon()
{
    NOTIMPLEMENTED();
    return QIcon();
}

// static
QIcon FilePlatformUtil::driveIcon(proto::DriveList::Item::Type type)
{
    NOTIMPLEMENTED();
    return QIcon();
}

// static
const QList<QChar>& FilePlatformUtil::invalidFileNameCharacters()
{
    static const QList<QChar> kInvalidCharacters =
        { '/', '\\', ':', '*', '?', '"', '<', '>', '|' };
    return kInvalidCharacters;
}

// static
const QList<QChar>& FilePlatformUtil::invalidPathCharacters()
{
    static const QList<QChar> kInvalidCharacters = { '*', '?', '"', '<', '>', '|' };
    return kInvalidCharacters;
}

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

// static
bool FilePlatformUtil::isRelativePath(const QString& /* path */)
{
    NOTIMPLEMENTED();
    return false;
}

// static
bool FilePlatformUtil::isNetworkPath(const QString& /* path */)
{
    NOTIMPLEMENTED();
    return false;
}

// static
bool FilePlatformUtil::isRootPath(const QString& /* path */)
{
    NOTIMPLEMENTED();
    return false;
}

} // namespace common
