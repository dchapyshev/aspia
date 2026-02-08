//
// SmartCafe Project
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

#include "router/migration_utils.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>

#include "base/logging.h"
#include "router/settings.h"

namespace router {

namespace {

//--------------------------------------------------------------------------------------------------
QString oldConfigFilePath()
{
#if defined(Q_OS_WINDOWS)
    return "C:\\ProgramData\\aspia\\router.json";
#else
    return "/etc/aspia/router.json";
#endif
}

//--------------------------------------------------------------------------------------------------
void doConfigMigrate(const QJsonDocument& doc)
{
    Settings settings;
    LOG(INFO) << "New config file:" << settings.filePath();

    QJsonObject root_object = doc.object();

    LOG(INFO) << "====== Migrate settings ======";

    if (root_object.isEmpty())
    {
        LOG(INFO) << "Root object is empty";
        return;
    }

    if (root_object.contains("AdminWhiteList"))
    {
        QString value = root_object["AdminWhiteList"].toString();
        LOG(INFO) << "AdminWhiteList:" << value;

        QStringList list = value.split(';', Qt::SkipEmptyParts);
        settings.setAdminWhiteList(list);
    }

    if (root_object.contains("ClientWhiteList"))
    {
        QString value = root_object["ClientWhiteList"].toString();
        LOG(INFO) << "ClientWhiteList:" << value;

        QStringList list = value.split(';', Qt::SkipEmptyParts);
        settings.setClientWhiteList(list);
    }

    if (root_object.contains("HostWhiteList"))
    {
        QString value = root_object["HostWhiteList"].toString();
        LOG(INFO) << "HostWhiteList:" << value;

        QStringList list = value.split(';', Qt::SkipEmptyParts);
        settings.setHostWhiteList(list);
    }

    if (root_object.contains("RelayWhiteList"))
    {
        QString value = root_object["RelayWhiteList"].toString();
        LOG(INFO) << "RelayWhiteList:" << value;

        QStringList list = value.split(';', Qt::SkipEmptyParts);
        settings.setRelayWhiteList(list);
    }

    if (root_object.contains("Port"))
    {
        quint16 value = root_object["Port"].toString().toUShort();
        LOG(INFO) << "Port:" << value;
        settings.setPort(value);
    }

    if (root_object.contains("PrivateKey"))
    {
        QString value = root_object["PrivateKey"].toString();
        LOG(INFO) << "PrivateKey:" << value;
        settings.setPrivateKey(QByteArray::fromHex(value.toLatin1()));
    }

    if (root_object.contains("SeedKey"))
    {
        QString value = root_object["SeedKey"].toString();
        LOG(INFO) << "SeedKey:" << value;
        settings.setSeedKey(QByteArray::fromHex(value.toLatin1()));
    }

    if (root_object.contains("ListenInterface"))
    {
        QString value = root_object["ListenInterface"].toString();
        LOG(INFO) << "ListenInterface:" << value;
        settings.setListenInterface(value);
    }
}

} // namespace

//--------------------------------------------------------------------------------------------------
bool isMigrationNeeded()
{
    QString old_config_file = oldConfigFilePath();
    bool has_old_config_file = false;

    if (QFileInfo::exists(old_config_file))
    {
        LOG(INFO) << "Old configuration file exists:" << old_config_file;
        has_old_config_file = true;
    }
    else
    {
        LOG(INFO) << "Old configuration file does NOT exist:" << old_config_file;
    }

    return has_old_config_file;
}

//--------------------------------------------------------------------------------------------------
void doMigration()
{
    QString old_config_file = oldConfigFilePath();
    if (QFileInfo::exists(old_config_file))
    {
        LOG(INFO) << "Start migration for" << old_config_file;

        QFile file(old_config_file);
        if (!file.open(QFile::ReadOnly))
        {
            LOG(ERROR) << "Unable to open file" << old_config_file << ":" << file.errorString();
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
                doConfigMigrate(doc);

                QString new_file_name = old_config_file + ".bak";

                if (QFile::rename(old_config_file, new_file_name))
                {
                    LOG(INFO) << "File" << old_config_file << "is renamed to" << new_file_name;
                }
                else
                {
                    LOG(ERROR) << "Unable to rename file from" << old_config_file << "to" << new_file_name;
                }
            }
        }
    }
}

} // namespace router
