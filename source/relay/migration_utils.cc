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

#include "relay/migration_utils.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>

#include "base/logging.h"
#include "relay/settings.h"

namespace relay {

namespace {

//--------------------------------------------------------------------------------------------------
QString oldConfigFilePath()
{
#if defined(Q_OS_WINDOWS)
    return "C:\\ProgramData\\aspia\\relay.json";
#else
    return "/etc/aspia/relay.json";
#endif
}

//--------------------------------------------------------------------------------------------------
void doConfigMigrate(const QJsonDocument& doc)
{
    Settings settings;
    LOG(INFO) << "New settings file:" << settings.filePath();

    QJsonObject root_object = doc.object();

    LOG(INFO) << "====== Migrate settings ======";

    if (root_object.isEmpty())
    {
        LOG(INFO) << "Root object is empty";
        return;
    }

    if (root_object.contains("RouterAddress"))
    {
        QString value = root_object["RouterAddress"].toString();
        LOG(INFO) << "RouterAddress:" << value;
        settings.setRouterAddress(value);
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

    if (root_object.contains("ListenInterface"))
    {
        QString value = root_object["ListenInterface"].toString();
        LOG(INFO) << "ListenInterface:" << value;
        settings.setListenInterface(value);
    }

    if (root_object.contains("PeerAddress"))
    {
        QString value = root_object["PeerAddress"].toString();
        LOG(INFO) << "PeerAddress:" << value;
        settings.setPeerAddress(value);
    }

    if (root_object.contains("PeerPort"))
    {
        quint16 value = root_object["PeerPort"].toString().toUShort();
        LOG(INFO) << "PeerPort:" << value;
        settings.setPeerPort(value);
    }

    if (root_object.contains("PeerIdleTimeout"))
    {
        qint64 value = root_object["PeerIdleTimeout"].toString().toLongLong();
        LOG(INFO) << "PeerIdleTimeout:" << value;
        settings.setPeerIdleTimeout(std::chrono::minutes(value));
    }

    if (root_object.contains("MaxPeerCount"))
    {
        quint32 value = root_object["MaxPeerCount"].toString().toULong();
        LOG(INFO) << "MaxPeerCount:" << value;
        settings.setMaxPeerCount(value);
    }

    if (root_object.contains("StatisticsEnabled"))
    {
        bool value = root_object["StatisticsEnabled"].toString() == "true";
        LOG(INFO) << "StatisticsEnabled:" << value;
        settings.setStatisticsEnabled(value);
    }

    if (root_object.contains("StatisticsInterval"))
    {
        qint64 value = root_object["StatisticsInterval"].toString().toLongLong();
        LOG(INFO) << "StatisticsInterval:" << value;
        settings.setStatisticsInterval(std::chrono::seconds(value));
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

} // namespace relay
