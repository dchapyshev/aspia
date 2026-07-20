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

#include "router/migration_utils.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>

#include <utility>

#include "base/logging.h"
#include "base/crypto/random.h"
#include "base/crypto/secure_byte_array.h"
#include "base/files/base_paths.h"
#include "router/settings.h"

namespace {

//--------------------------------------------------------------------------------------------------
QString oldConfigFilePath()
{
    return BasePaths::appConfigDir() + "/router.json";
}

//--------------------------------------------------------------------------------------------------
// QByteArray::fromHex silently skips invalid characters, so a corrupted value would migrate into a
// truncated (or empty) key without any error. Reject such input via a round-trip check and return
// an empty array, letting the caller skip the key instead of storing broken material.
QByteArray decodeHexKey(const QString& value)
{
    const QByteArray hex = value.toLatin1();
    const QByteArray decoded = QByteArray::fromHex(hex);
    if (decoded.isEmpty() || decoded.toHex() != hex.toLower())
        return QByteArray();
    return decoded;
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
        settings.setLegacyPort(value);
    }

    if (root_object.contains("PrivateKey"))
    {
        QByteArray decoded = decodeHexKey(root_object["PrivateKey"].toString());
        if (decoded.isEmpty())
        {
            LOG(ERROR) << "Invalid PrivateKey in old config, skipping its migration";
        }
        else
        {
            // Hosts and relays are separate entities now, but the old config had a single key.
            // Duplicate it into both so existing hosts and relays keep working after migration.
            SecureByteArray private_key(std::move(decoded));
            settings.setHostPrivateKey(private_key);
            settings.setRelayPrivateKey(private_key);
        }
    }

    if (root_object.contains("SeedKey"))
    {
        QByteArray decoded = decodeHexKey(root_object["SeedKey"].toString());
        if (decoded.isEmpty())
            LOG(ERROR) << "Invalid SeedKey in old config, skipping its migration";
        else
            settings.setSeedKey(decoded);
    }

    if (root_object.contains("ListenInterface"))
    {
        QString value = root_object["ListenInterface"].toString();
        LOG(INFO) << "ListenInterface:" << value;
        settings.setListenInterface(value);
    }

    // Values the old configuration may lack; issue them while write access is available.
    if (settings.seedKey().isEmpty())
        settings.setSeedKey(Random::byteArray(64));
    settings.setRouterGuid(QUuid::createUuid().toString(QUuid::WithoutBraces));
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
