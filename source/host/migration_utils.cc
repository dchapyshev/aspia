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

#include "host/migration_utils.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>

#include <qt_windows.h>
#include <ShlObj.h>

#include "base/logging.h"
#include "base/peer/user_list.h"
#include "host/host_storage.h"
#include "host/system_settings.h"

namespace host {

namespace {

//--------------------------------------------------------------------------------------------------
QString configDir()
{
    wchar_t buffer[MAX_PATH] = { 0 };

    HRESULT hr = SHGetFolderPathW(nullptr, CSIDL_COMMON_APPDATA,
                                  nullptr, SHGFP_TYPE_CURRENT, buffer);
    if (FAILED(hr))
    {
        LOG(ERROR) << "SHGetFolderPathW failed";
        return QString();
    }

    return QString::fromWCharArray(buffer);
}

//--------------------------------------------------------------------------------------------------
QString hostFilePath()
{
    return configDir() + "\\aspia\\host.json";
}

//--------------------------------------------------------------------------------------------------
QString hostKeyFilePath()
{
    return configDir() + "\\aspia\\host_key.json";
}

//--------------------------------------------------------------------------------------------------
QString hostIpcFilePath()
{
    return configDir() + "\\aspia\\host_ipc.json";
}

//--------------------------------------------------------------------------------------------------
void doHostMigrate(const QJsonDocument& doc)
{
    SystemSettings settings;

    QJsonObject root_object = doc.object();

    LOG(INFO) << "====== Migrate settings ======";

    if (root_object.isEmpty())
    {
        LOG(INFO) << "Root object is empty";
        return;
    }

    if (root_object.contains("ApplicationShutdownDisabled"))
    {
        bool value = root_object["ApplicationShutdownDisabled"].toString() == "true";
        LOG(INFO) << "ApplicationShutdownDisabled:" << value;
        settings.setApplicationShutdownDisabled(value);
    }

    if (root_object.contains("AutoConnConfirmInterval"))
    {
        qint64 value = root_object["AutoConnConfirmInterval"].toString().toLongLong();
        LOG(INFO) << "AutoConnConfirmInterval:" << value;
        settings.setAutoConfirmationInterval(std::chrono::milliseconds(value));
    }

    if (root_object.contains("AutoUpdateEnabled"))
    {
        bool value = root_object["AutoUpdateEnabled"].toString() == "true";
        LOG(INFO) << "AutoUpdateEnabled:" << value;
        settings.setAutoUpdateEnabled(value);
    }

    if (root_object.contains("ConnConfirm"))
    {
        bool value = root_object["ConnConfirm"].toString() == "true";
        LOG(INFO) << "ConnConfirm:" << value;
        settings.setConnectConfirmation(value);
    }

    if (root_object.contains("ConnConfirmNoUserAction"))
    {
        int value = root_object["ConnConfirmNoUserAction"].toString().toInt();
        LOG(INFO) << "ConnConfirmNoUserAction:" << value;
        settings.setNoUserAction(static_cast<SystemSettings::NoUserAction>(value));
    }

    if (root_object.contains("OneTimePassword"))
    {
        bool value = root_object["OneTimePassword"].toString() == "true";
        LOG(INFO) << "OneTimePassword:" << value;
        settings.setOneTimePassword(value);
    }

    if (root_object.contains("OneTimePasswordCharacters"))
    {
        quint32 value = root_object["OneTimePasswordCharacters"].toString().toUInt();
        LOG(INFO) << "OneTimePasswordCharacters:" << value;
        settings.setOneTimePasswordCharacters(value);
    }

    if (root_object.contains("OneTimePasswordExpire"))
    {
        qint64 value = root_object["OneTimePasswordExpire"].toString().toLongLong();
        LOG(INFO) << "OneTimePasswordExpire:" << value;
        settings.setOneTimePasswordExpire(std::chrono::milliseconds(value));
    }

    if (root_object.contains("OneTimePasswordLength"))
    {
        int value = root_object["OneTimePasswordLength"].toString().toInt();
        LOG(INFO) << "OneTimePasswordLength:" << value;
        settings.setOneTimePasswordLength(value);
    }

    if (root_object.contains("RouterAddress"))
    {
        QString value = root_object["RouterAddress"].toString();
        LOG(INFO) << "RouterAddress:" << value;
        settings.setRouterAddress(value);
    }

    if (root_object.contains("RouterEnabled"))
    {
        bool value = root_object["RouterEnabled"].toString() == "true";
        LOG(INFO) << "RouterEnabled:" << value;
        settings.setRouterEnabled(value);
    }

    if (root_object.contains("RouterPort"))
    {
        quint16 value = root_object["RouterPort"].toString().toUShort();
        LOG(INFO) << "RouterPort:" << value;
        settings.setRouterPort(value);
    }

    if (root_object.contains("RouterPublicKey"))
    {
        QString value = root_object["RouterPublicKey"].toString();
        LOG(INFO) << "RouterPublicKey:" << value;
        settings.setRouterPublicKey(QByteArray::fromHex(value.toLatin1()));
    }

    if (root_object.contains("TcpPort"))
    {
        quint16 value = root_object["TcpPort"].toString().toUShort();
        LOG(INFO) << "TcpPort:" << value;
        settings.setTcpPort(value);
    }

    if (root_object.contains("UpdateCheckFrequency"))
    {
        int value = root_object["UpdateCheckFrequency"].toString().toInt();
        LOG(INFO) << "UpdateCheckFrequency:" << value;
        settings.setUpdateCheckFrequency(value);
    }

    if (root_object.contains("UpdateServer"))
    {
        QString value = root_object["UpdateServer"].toString();
        LOG(INFO) << "UpdateServer:" << value;
        settings.setUpdateServer(value);
    }

    LOG(INFO) << "====== Migrate user list ======";

    std::unique_ptr<base::UserList> user_list = base::UserList::createEmpty();

    if (root_object.contains("SeedKey"))
    {
        QString value = root_object["SeedKey"].toString();
        LOG(INFO) << "SeedKey:" << value;
        user_list->setSeedKey(QByteArray::fromHex(value.toLatin1()));
    }

    QJsonObject users_object = root_object["Users"].toObject();
    if (!users_object.isEmpty())
    {
        int user_count = users_object["size"].toString().toInt();
        LOG(INFO) << "User count:" << user_count;

        for (int i = 1; i <= user_count; ++i)
        {
            QJsonObject user_object = users_object[QString::number(i)].toObject();
            base::User user;

            LOG(INFO) << "=== USER#" << i << "===";

            if (user_object.contains("Flags"))
            {
                int value = user_object["Flags"].toString().toInt();
                LOG(INFO) << "Flags:" << value;
                user.flags = value;
            }

            if (user_object.contains("Group"))
            {
                QString value = user_object["Group"].toString();
                LOG(INFO) << "Group:" << value;
                user.group = value;
            }

            if (user_object.contains("Name"))
            {
                QString value = user_object["Name"].toString();
                LOG(INFO) << "Name:" << value;
                user.name = value;
            }

            if (user_object.contains("Salt"))
            {
                QString value = user_object["Salt"].toString();
                LOG(INFO) << "Salt:" << value;
                user.salt = QByteArray::fromHex(value.toLatin1());
            }

            if (user_object.contains("Sessions"))
            {
                int value = user_object["Sessions"].toString().toInt();
                LOG(INFO) << "Sessions:" << value;
                user.sessions = value;
            }

            if (user_object.contains("Verifier"))
            {
                QString value = user_object["Verifier"].toString();
                LOG(INFO) << "Verifier:" << value;
                user.verifier = QByteArray::fromHex(value.toLatin1());
            }

            if (user.isValid())
            {
                LOG(INFO) << "Add user" << i << ":" << user.name;
                user_list->add(user);
            }
            else
            {
                LOG(ERROR) << "Invalid user" << i;
            }
        }
    }

    settings.setUserList(*user_list);
}

//--------------------------------------------------------------------------------------------------
void doHostKeyMigrate(const QJsonDocument& doc)
{
    HostStorage storage;
    QJsonObject root_object = doc.object();

    LOG(INFO) << "====== Migrate host key ======";

    if (root_object.isEmpty())
    {
        LOG(INFO) << "Root object is empty";
        return;
    }

    if (root_object.contains("console"))
    {
        QString value = root_object["console"].toString();
        LOG(INFO) << "console:" << value;
        storage.setHostKey(QByteArray::fromHex(value.toLatin1()));
    }
}

} // namespace

//--------------------------------------------------------------------------------------------------
// static
bool MigrationUtils::isMigrationNeeded()
{
    QString host_file = hostFilePath();
    bool has_host_file = false;

    QString host_key_file = hostKeyFilePath();
    bool has_host_key_file = false;

    QString host_ipc_file = hostIpcFilePath();
    bool has_host_ipc_file = false;

    if (QFileInfo::exists(host_file))
    {
        LOG(INFO) << "Old configuration file exists:" << host_file;
        has_host_file = true;
    }
    else
    {
        LOG(INFO) << "Old configuration file does NOT exist:" << host_file;
    }

    if (QFileInfo::exists(host_key_file))
    {
        LOG(INFO) << "Old key file exists:" << host_key_file;
        has_host_key_file = true;
    }
    else
    {
        LOG(INFO) << "Old key file does NOT exist:" << host_key_file;
    }

    if (QFileInfo::exists(host_ipc_file))
    {
        LOG(INFO) << "Old IPC file exists:" << host_ipc_file;
        has_host_ipc_file = true;
    }
    else
    {
        LOG(INFO) << "Old IPC file does NOT exist:" << host_ipc_file;
    }

    return has_host_file || has_host_key_file || has_host_ipc_file;
}

//--------------------------------------------------------------------------------------------------
// static
void MigrationUtils::doMigrate()
{
    LOG(INFO) << "Start migration";

    QString host_file = hostFilePath();
    QString host_key_file = hostKeyFilePath();
    QString host_ipc_file = hostIpcFilePath();

    if (QFileInfo::exists(host_file))
    {
        LOG(INFO) << "Start migration for" << host_file;

        QFile file(host_file);
        if (!file.open(QFile::ReadOnly))
        {
            LOG(ERROR) << "Unable to open file" << host_file << ":" << file.errorString();
        }
        else
        {
            QByteArray buffer = file.readAll();
            file.close();

            QJsonParseError parse_error;
            QJsonDocument doc = QJsonDocument::fromJson(buffer, &parse_error);
            if (parse_error.error != QJsonParseError::NoError)
            {
                LOG(ERROR) << "JSON parse error at" << parse_error.offset << ":"
                           << parse_error.errorString();
            }
            else
            {
                doHostMigrate(doc);

                QString new_file_name = host_file + ".bak";

                if (QFile::rename(host_file, new_file_name))
                {
                    LOG(INFO) << "File" << host_file << "is renamed to" << new_file_name;
                }
                else
                {
                    LOG(ERROR) << "Unable to rename file from" << host_file << "to" << new_file_name;
                }
            }
        }
    }

    if (QFileInfo::exists(host_key_file))
    {
        LOG(INFO) << "Start migration for" << host_key_file;

        QFile file(host_key_file);
        if (!file.open(QFile::ReadOnly))
        {
            LOG(ERROR) << "Unable to open file" << host_key_file << ":" << file.errorString();
        }
        else
        {
            QByteArray buffer = file.readAll();
            file.close();

            QJsonParseError parse_error;
            QJsonDocument doc = QJsonDocument::fromJson(buffer, &parse_error);
            if (parse_error.error != QJsonParseError::NoError)
            {
                LOG(ERROR) << "JSON parse error at" << parse_error.offset << ":"
                           << parse_error.errorString();
            }
            else
            {
                doHostKeyMigrate(doc);

                QString new_file_name = host_key_file + ".bak";

                if (QFile::rename(host_key_file, new_file_name))
                {
                    LOG(INFO) << "File" << host_key_file << "is renamed to" << new_file_name;
                }
                else
                {
                    LOG(ERROR) << "Unable to rename file from" << host_key_file << "to" << new_file_name;
                }
            }
        }
    }

    if (QFileInfo::exists(host_ipc_file))
    {
        LOG(INFO) << "Start migration for" << host_key_file;

        QString new_file_name = host_ipc_file + ".bak";

        if (QFile::rename(host_ipc_file, new_file_name))
        {
            LOG(INFO) << "File" << host_ipc_file << "is renamed to" << new_file_name;
        }
        else
        {
            LOG(ERROR) << "Unable to rename file from" << host_ipc_file << "to" << new_file_name;
        }
    }

    LOG(INFO) << "Migration is DONE";
}

} // namespace host
