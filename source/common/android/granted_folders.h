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

#ifndef COMMON_ANDROID_GRANTED_FOLDERS_H
#define COMMON_ANDROID_GRANTED_FOLDERS_H

#include <QList>
#include <QString>

// Folders of the device storage the user gave the application through the system picker.
class GrantedFolders
{
public:
    struct Folder
    {
        QString path;
        QString uri;
    };

    static QList<Folder> list();

    // Keeps the access to the folder |uri| returned by the system picker. Returns false for a folder
    // outside the device storage.
    static bool add(const QString& uri);
    static void remove(const QString& uri);

    // Returns true when |path| lies inside a granted folder and has no "." and ".." names in it.
    static bool contains(const QString& path);

    // The name the file engine opens |path| by. A path inside a granted folder is reached through the
    // document provider of the folder, other paths are returned as they are.
    static QString accessPath(const QString& path);

private:
    Q_DISABLE_COPY_MOVE(GrantedFolders)
};

#endif // COMMON_ANDROID_GRANTED_FOLDERS_H
