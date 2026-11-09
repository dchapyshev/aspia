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

#include "common/android/granted_folders.h"

#include <QCoreApplication>
#include <QHash>
#include <QJniObject>
#include <QUrl>

#include "base/logging.h"

namespace {

const char kExternalStorageAuthority[] = "com.android.externalstorage.documents";

// Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION.
const jint kReadWriteFlags = 0x00000001 | 0x00000002;

//--------------------------------------------------------------------------------------------------
QJniObject contentResolver()
{
    QJniObject context = QNativeInterface::QAndroidApplication::context();
    if (!context.isValid())
        return QJniObject();

    return context.callObjectMethod("getContentResolver", "()Landroid/content/ContentResolver;");
}

//--------------------------------------------------------------------------------------------------
QJniObject parseUri(const QString& uri)
{
    return QJniObject::callStaticObjectMethod("android/net/Uri", "parse",
        "(Ljava/lang/String;)Landroid/net/Uri;", QJniObject::fromString(uri).object<jstring>());
}

//--------------------------------------------------------------------------------------------------
// The granted folder |path| lies in. A folder granted inside another granted folder is the closer one.
const GrantedFolders::Folder* findFolder(const QList<GrantedFolders::Folder>& folders, const QString& path)
{
    const GrantedFolders::Folder* folder = nullptr;

    for (const GrantedFolders::Folder& candidate : folders)
    {
        if (candidate.path != path && !path.startsWith(candidate.path + '/'))
            continue;

        if (!folder || candidate.path.size() > folder->path.size())
            folder = &candidate;
    }

    return folder;
}

} // namespace

//--------------------------------------------------------------------------------------------------
// static
QList<GrantedFolders::Folder> GrantedFolders::list()
{
    QList<Folder> folders;

    QJniObject context = QNativeInterface::QAndroidApplication::context();
    if (!context.isValid())
        return folders;

    // A document tree names its volume "primary" or by the UUID of the volume.
    QHash<QString, QString> volume_paths;

    QJniObject storage_manager = context.callObjectMethod("getSystemService",
        "(Ljava/lang/String;)Ljava/lang/Object;", QJniObject::fromString("storage").object<jstring>());
    QJniObject volumes;
    if (storage_manager.isValid())
        volumes = storage_manager.callObjectMethod("getStorageVolumes", "()Ljava/util/List;");

    const int volume_count = volumes.isValid() ? volumes.callMethod<jint>("size") : 0;
    for (int i = 0; i < volume_count; ++i)
    {
        QJniObject volume = volumes.callObjectMethod("get", "(I)Ljava/lang/Object;", i);
        QJniObject directory = volume.callObjectMethod("getDirectory", "()Ljava/io/File;");
        if (!directory.isValid())
            continue;

        QString id("primary");
        if (!volume.callMethod<jboolean>("isPrimary"))
            id = volume.callObjectMethod("getUuid", "()Ljava/lang/String;").toString();

        volume_paths.insert(id, directory.callObjectMethod("getAbsolutePath", "()Ljava/lang/String;").toString());
    }

    QJniObject resolver = contentResolver();
    QJniObject permissions;
    if (resolver.isValid())
        permissions = resolver.callObjectMethod("getPersistedUriPermissions", "()Ljava/util/List;");

    const int permission_count = permissions.isValid() ? permissions.callMethod<jint>("size") : 0;
    for (int i = 0; i < permission_count; ++i)
    {
        QJniObject permission = permissions.callObjectMethod("get", "(I)Ljava/lang/Object;", i);
        if (!permission.callMethod<jboolean>("isReadPermission"))
            continue;

        QJniObject uri = permission.callObjectMethod("getUri", "()Landroid/net/Uri;");
        if (uri.callObjectMethod("getAuthority", "()Ljava/lang/String;").toString() != kExternalStorageAuthority)
            continue;

        if (!QJniObject::callStaticMethod<jboolean>(
                "android/provider/DocumentsContract", "isTreeUri", "(Landroid/net/Uri;)Z", uri.object()))
        {
            continue;
        }

        // "primary:Documents/Work" is the folder Documents/Work of the primary volume.
        const QString tree_id = QJniObject::callStaticObjectMethod("android/provider/DocumentsContract",
            "getTreeDocumentId", "(Landroid/net/Uri;)Ljava/lang/String;", uri.object()).toString();

        const qsizetype separator = tree_id.indexOf(':');
        if (separator == -1)
            continue;

        const QString volume_path = volume_paths.value(tree_id.left(separator));
        if (volume_path.isEmpty())
            continue;

        const QString relative_path = tree_id.mid(separator + 1);

        Folder folder;
        folder.path = relative_path.isEmpty() ? volume_path : volume_path + '/' + relative_path;
        folder.uri = uri.toString();
        folders.append(folder);
    }

    return folders;
}

//--------------------------------------------------------------------------------------------------
// static
bool GrantedFolders::add(const QString& uri)
{
    QJniObject resolver = contentResolver();
    QJniObject parsed_uri = parseUri(uri);
    if (!resolver.isValid() || !parsed_uri.isValid())
        return false;

    // The picker leaves the folder readable and writable until the application exits. Keeping the
    // access over restarts is a separate step.
    resolver.callMethod<void>("takePersistableUriPermission", "(Landroid/net/Uri;I)V",
                              parsed_uri.object(), kReadWriteFlags);

    const QList<Folder> folders = list();
    for (const Folder& folder : folders)
    {
        if (folder.uri == uri)
            return true;
    }

    LOG(WARNING) << "Folder is not accessible or is outside the device storage:" << uri;
    remove(uri);
    return false;
}

//--------------------------------------------------------------------------------------------------
// static
void GrantedFolders::remove(const QString& uri)
{
    QJniObject resolver = contentResolver();
    QJniObject parsed_uri = parseUri(uri);
    if (!resolver.isValid() || !parsed_uri.isValid())
        return;

    resolver.callMethod<void>("releasePersistableUriPermission", "(Landroid/net/Uri;I)V",
                              parsed_uri.object(), kReadWriteFlags);
}

//--------------------------------------------------------------------------------------------------
// static
bool GrantedFolders::contains(const QString& path)
{
    const QStringList names = path.split('/', Qt::SkipEmptyParts);
    if (names.contains(".") || names.contains(".."))
        return false;

    return findFolder(list(), path) != nullptr;
}

//--------------------------------------------------------------------------------------------------
// static
QString GrantedFolders::accessPath(const QString& path)
{
    const QList<Folder> folders = list();

    const Folder* folder = findFolder(folders, path);
    if (!folder)
        return path;

    QString uri = folder->uri;

    const QStringList names = path.mid(folder->path.size()).split('/', Qt::SkipEmptyParts);
    for (const QString& name : names)
        uri += '/' + QString::fromLatin1(QUrl::toPercentEncoding(name));

    return uri;
}
