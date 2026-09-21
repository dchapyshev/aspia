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

#include "common/android/log_archiver.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJniEnvironment>
#include <QJniObject>

#include "base/logging.h"

namespace {

const qint64 kChunkSize = 64 * 1024;

// The version of Android the downloads of the device became writable through the store of the media in.
const int kMediaStoreVersion = 29;

//--------------------------------------------------------------------------------------------------
QString archiveName()
{
    QJniObject context = QNativeInterface::QAndroidApplication::context();
    if (!context.isValid())
        return QString();

    QString package =
        context.callObjectMethod("getPackageName", "()Ljava/lang/String;").toString();

    return QString("aspia-%1-logs-%2.zip")
        .arg(package.section('.', -1), QDateTime::currentDateTime().toString("yyyyMMdd-hhmmss"));
}

//--------------------------------------------------------------------------------------------------
void putValue(const QJniObject& values, const char* key, const QString& value)
{
    values.callMethod<void>("put", "(Ljava/lang/String;Ljava/lang/String;)V",
                            QJniObject::fromString(QLatin1StringView(key)).object<jstring>(),
                            QJniObject::fromString(value).object<jstring>());
}

//--------------------------------------------------------------------------------------------------
// Opens the stream the archive is written into and names in |location| the place it goes to.
QJniObject openStream(const QString& name, QString* location)
{
    QJniObject context = QNativeInterface::QAndroidApplication::context();
    if (!context.isValid())
    {
        LOG(ERROR) << "Invalid context";
        return QJniObject();
    }

    QJniEnvironment env;

    // The downloads are storage shared by the whole device, and an application reaches them through
    // the store of the media. Before Android 10 there is no such way in, so the archive is left in
    // the directory of the application, where its logs already are.
    if (QJniObject::getStaticField<jint>("android/os/Build$VERSION", "SDK_INT") < kMediaStoreVersion)
    {
        QJniObject dir = context.callObjectMethod(
            "getExternalFilesDir", "(Ljava/lang/String;)Ljava/io/File;", nullptr);
        if (env.checkAndClearExceptions() || !dir.isValid())
        {
            LOG(ERROR) << "Unable to get the directory of the application";
            return QJniObject();
        }

        QString path = dir.callObjectMethod<jstring>("getAbsolutePath").toString() + "/" + name;

        QJniObject stream("java/io/FileOutputStream", "(Ljava/lang/String;)V",
                          QJniObject::fromString(path).object<jstring>());
        if (env.checkAndClearExceptions() || !stream.isValid())
        {
            LOG(ERROR) << "Unable to create file:" << path;
            return QJniObject();
        }

        *location = path;
        return stream;
    }

    QJniObject values("android/content/ContentValues");
    putValue(values, "_display_name", name);
    putValue(values, "mime_type", "application/zip");
    putValue(values, "relative_path", "Download");

    QJniObject resolver =
        context.callObjectMethod("getContentResolver", "()Landroid/content/ContentResolver;");
    QJniObject collection = QJniObject::getStaticObjectField(
        "android/provider/MediaStore$Downloads", "EXTERNAL_CONTENT_URI", "Landroid/net/Uri;");

    QJniObject uri = resolver.callObjectMethod(
        "insert", "(Landroid/net/Uri;Landroid/content/ContentValues;)Landroid/net/Uri;",
        collection.object(), values.object());
    if (env.checkAndClearExceptions() || !uri.isValid())
    {
        LOG(ERROR) << "Unable to create the record of the file";
        return QJniObject();
    }

    QJniObject stream = resolver.callObjectMethod(
        "openOutputStream", "(Landroid/net/Uri;)Ljava/io/OutputStream;", uri.object());
    if (env.checkAndClearExceptions() || !stream.isValid())
    {
        LOG(ERROR) << "Unable to open the stream of the file";
        return QJniObject();
    }

    *location = "Download/" + name;
    return stream;
}

//--------------------------------------------------------------------------------------------------
// Writes the file |name| from |dir_path| into the archive under the same name.
bool writeEntry(const QJniObject& zip, const QString& dir_path, const QString& name)
{
    QFile file(dir_path + "/" + name);
    if (!file.open(QIODevice::ReadOnly))
    {
        LOG(ERROR) << "Unable to open file:" << file.errorString();
        return false;
    }

    QJniEnvironment env;

    QJniObject entry("java/util/zip/ZipEntry", "(Ljava/lang/String;)V",
                     QJniObject::fromString(name).object<jstring>());

    zip.callMethod<void>("putNextEntry", "(Ljava/util/zip/ZipEntry;)V", entry.object());
    if (env.checkAndClearExceptions())
    {
        LOG(ERROR) << "Unable to add file to the archive:" << name;
        return false;
    }

    while (!file.atEnd())
    {
        const QByteArray chunk = file.read(kChunkSize);
        if (chunk.isEmpty())
            break;

        const jsize size = static_cast<jsize>(chunk.size());

        // The stream of the archive belongs to the machine of Java and takes what it writes as an
        // array of that machine, so the piece read is copied into one.
        jbyteArray buffer = env->NewByteArray(size);
        if (!buffer)
        {
            LOG(ERROR) << "Unable to allocate the buffer";
            return false;
        }

        env->SetByteArrayRegion(buffer, 0, size, reinterpret_cast<const jbyte*>(chunk.constData()));
        zip.callMethod<void>("write", "([BII)V", buffer, 0, size);
        env->DeleteLocalRef(buffer);

        if (env.checkAndClearExceptions())
        {
            LOG(ERROR) << "Unable to write file to the archive:" << name;
            return false;
        }
    }

    zip.callMethod<void>("closeEntry", "()V");
    return !env.checkAndClearExceptions();
}

} // namespace

//--------------------------------------------------------------------------------------------------
// static
LogArchiver::Result LogArchiver::saveToDownloads(QString* location)
{
    const QString dir_path = loggingDirectory();
    const QStringList names =
        QDir(dir_path).entryList(QStringList() << "*.log", QDir::Files, QDir::Name);

    if (names.isEmpty())
    {
        LOG(INFO) << "No log files in" << dir_path;
        return Result::NO_LOGS;
    }

    const QString name = archiveName();
    if (name.isEmpty())
        return Result::FAILED;

    QJniObject stream = openStream(name, location);
    if (!stream.isValid())
        return Result::FAILED;

    QJniEnvironment env;

    QJniObject zip("java/util/zip/ZipOutputStream", "(Ljava/io/OutputStream;)V", stream.object());
    if (env.checkAndClearExceptions() || !zip.isValid())
    {
        LOG(ERROR) << "Unable to create the archive";
        return Result::FAILED;
    }

    bool written = true;

    for (const QString& file_name : names)
    {
        if (!writeEntry(zip, dir_path, file_name))
        {
            written = false;
            break;
        }
    }

    // The archive is closed even when a file did not make it, because the stream stays open for as
    // long as this object does.
    zip.callMethod<void>("close", "()V");

    if (env.checkAndClearExceptions() || !written)
    {
        LOG(ERROR) << "Unable to close the archive";
        return Result::FAILED;
    }

    LOG(INFO) << "Logs are saved to" << *location;
    return Result::SUCCESS;
}
