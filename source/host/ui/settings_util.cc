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

#include "host/ui/settings_util.h"

#include <QAbstractButton>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "base/logging.h"
#include "base/net/address.h"
#include "base/peer/user.h"
#include "common/desktop/msg_box.h"
#include "host/database.h"
#include "host/system_settings.h"

namespace {

const char kSystem[] = "system";
const char kDatabase[] = "database";

const char kUpdateServer[] = "update_server";
const char kPreferredVideoCapturer[] = "preferred_video_capturer";
const char kApplicationShutdownDisabled[] = "application_shutdown_disabled";
const char kAutoUpdateEnabled[] = "auto_update_enabled";
const char kUpdateCheckFrequency[] = "update_check_frequency";

const char kSeedKey[] = "seed_key";
const char kTcpPort[] = "tcp_port";
const char kRouterEnabled[] = "router_enabled";
const char kRouterAddress[] = "router_address";
const char kRouterPublicKey[] = "router_public_key";
const char kConnectConfirmation[] = "connect_confirmation";
const char kNoUserAction[] = "no_user_action";
const char kAutoConfirmationInterval[] = "auto_confirmation_interval";
const char kOneTimePassword[] = "one_time_password";
const char kOneTimePasswordExpire[] = "one_time_password_expire";
const char kOneTimePasswordLength[] = "one_time_password_length";
const char kOneTimePasswordCharacters[] = "one_time_password_characters";
const char kUsers[] = "users";

const char kUserName[] = "name";
const char kUserGroup[] = "group";
const char kUserSalt[] = "salt";
const char kUserVerifier[] = "verifier";
const char kUserSessions[] = "sessions";
const char kUserFlags[] = "flags";

//--------------------------------------------------------------------------------------------------
QJsonObject exportSystemSettings()
{
    SystemSettings settings;

    QJsonObject obj;
    obj[kUpdateServer] = settings.updateServer();
    obj[kPreferredVideoCapturer] = static_cast<qint64>(settings.preferredVideoCapturer());
    obj[kApplicationShutdownDisabled] = settings.isApplicationShutdownDisabled();
    obj[kAutoUpdateEnabled] = settings.isAutoUpdateEnabled();
    obj[kUpdateCheckFrequency] = settings.updateCheckFrequency();
    return obj;
}

//--------------------------------------------------------------------------------------------------
QJsonObject exportDatabase()
{
    Database& db = Database::instance();

    QJsonObject obj;
    obj[kSeedKey] = QString::fromLatin1(db.seedKey().toHex());
    obj[kTcpPort] = db.tcpPort();
    obj[kRouterEnabled] = db.isRouterEnabled();
    obj[kRouterAddress] = db.routerAddress().toString();
    obj[kRouterPublicKey] = QString::fromLatin1(db.routerPublicKey().toHex());
    obj[kConnectConfirmation] = db.connectConfirmation();
    obj[kNoUserAction] = static_cast<int>(db.noUserAction());
    obj[kAutoConfirmationInterval] = static_cast<qint64>(db.autoConfirmationInterval().count());
    obj[kOneTimePassword] = db.oneTimePassword();
    obj[kOneTimePasswordExpire] = static_cast<qint64>(db.oneTimePasswordExpire().count());
    obj[kOneTimePasswordLength] = db.oneTimePasswordLength();
    obj[kOneTimePasswordCharacters] = static_cast<qint64>(db.oneTimePasswordCharacters());

    QJsonArray users_array;
    const QVector<User> users = db.userList();
    for (const User& user : users)
    {
        QJsonObject user_obj;
        user_obj[kUserName] = user.name;
        user_obj[kUserGroup] = user.group;
        user_obj[kUserSalt] = QString::fromLatin1(user.salt.toHex());
        user_obj[kUserVerifier] = QString::fromLatin1(user.verifier.toHex());
        user_obj[kUserSessions] = static_cast<qint64>(user.sessions);
        user_obj[kUserFlags] = static_cast<qint64>(user.flags);
        users_array.append(user_obj);
    }
    obj[kUsers] = users_array;

    return obj;
}

//--------------------------------------------------------------------------------------------------
void importSystemSettings(const QJsonObject& obj)
{
    SystemSettings settings;

    if (obj.contains(kUpdateServer))
        settings.setUpdateServer(obj[kUpdateServer].toString());
    if (obj.contains(kPreferredVideoCapturer))
        settings.setPreferredVideoCapturer(static_cast<quint32>(obj[kPreferredVideoCapturer].toInteger()));
    if (obj.contains(kApplicationShutdownDisabled))
        settings.setApplicationShutdownDisabled(obj[kApplicationShutdownDisabled].toBool());
    if (obj.contains(kAutoUpdateEnabled))
        settings.setAutoUpdateEnabled(obj[kAutoUpdateEnabled].toBool());
    if (obj.contains(kUpdateCheckFrequency))
        settings.setUpdateCheckFrequency(obj[kUpdateCheckFrequency].toInt());

    settings.sync();
}

//--------------------------------------------------------------------------------------------------
bool importDatabase(const QJsonObject& obj)
{
    Database& db = Database::instance();
    if (!db.isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    if (obj.contains(kSeedKey))
        db.setSeedKey(QByteArray::fromHex(obj[kSeedKey].toString().toLatin1()));
    if (obj.contains(kTcpPort))
        db.setTcpPort(static_cast<quint16>(obj[kTcpPort].toInt()));
    if (obj.contains(kRouterEnabled))
        db.setRouterEnabled(obj[kRouterEnabled].toBool());
    if (obj.contains(kRouterAddress))
        db.setRouterAddress(Address::fromString(obj[kRouterAddress].toString(), DEFAULT_ROUTER_TCP_PORT));
    if (obj.contains(kRouterPublicKey))
        db.setRouterPublicKey(QByteArray::fromHex(obj[kRouterPublicKey].toString().toLatin1()));
    if (obj.contains(kConnectConfirmation))
        db.setConnectConfirmation(obj[kConnectConfirmation].toBool());
    if (obj.contains(kNoUserAction))
        db.setNoUserAction(static_cast<Database::NoUserAction>(obj[kNoUserAction].toInt()));
    if (obj.contains(kAutoConfirmationInterval))
        db.setAutoConfirmationInterval(std::chrono::milliseconds(obj[kAutoConfirmationInterval].toInteger()));
    if (obj.contains(kOneTimePassword))
        db.setOneTimePassword(obj[kOneTimePassword].toBool());
    if (obj.contains(kOneTimePasswordExpire))
        db.setOneTimePasswordExpire(std::chrono::milliseconds(obj[kOneTimePasswordExpire].toInteger()));
    if (obj.contains(kOneTimePasswordLength))
        db.setOneTimePasswordLength(obj[kOneTimePasswordLength].toInt());
    if (obj.contains(kOneTimePasswordCharacters))
        db.setOneTimePasswordCharacters(static_cast<quint32>(obj[kOneTimePasswordCharacters].toInteger()));

    if (obj.contains(kUsers))
    {
        const QVector<User> existing = db.userList();
        for (const User& user : existing)
            db.removeUser(user.entry_id);

        const QJsonArray users_array = obj[kUsers].toArray();
        for (const QJsonValue& value : users_array)
        {
            QJsonObject user_obj = value.toObject();

            User user;
            user.name = user_obj[kUserName].toString();
            user.group = user_obj[kUserGroup].toString();
            user.salt = QByteArray::fromHex(user_obj[kUserSalt].toString().toLatin1());
            user.verifier = QByteArray::fromHex(user_obj[kUserVerifier].toString().toLatin1());
            user.sessions = static_cast<quint32>(user_obj[kUserSessions].toInteger());
            user.flags = static_cast<quint32>(user_obj[kUserFlags].toInteger());

            if (!user.isValid())
            {
                LOG(WARNING) << "Skipping invalid user:" << user.name;
                continue;
            }

            db.addUser(user);
        }
    }

    return true;
}

} // namespace

//--------------------------------------------------------------------------------------------------
// static
bool SettingsUtil::importFromFile(const QString& path, bool silent, QWidget* parent)
{
    LOG(INFO) << "Import settings from" << path;

    if (!QFileInfo::exists(path))
    {
        LOG(ERROR) << "Source settings file does not exist";
        if (!silent)
            MsgBox::warning(parent, tr("Source settings file does not exist."));
        return false;
    }

    QFile file(path);
    if (!file.open(QFile::ReadOnly))
    {
        LOG(ERROR) << "Unable to open source file:" << file.errorString();
        if (!silent)
            MsgBox::warning(parent, tr("Unable to open the source file."));
        return false;
    }

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (doc.isNull() || !doc.isObject())
    {
        LOG(ERROR) << "Failed to parse source file:" << error.errorString();
        if (!silent)
        {
            MsgBox::warning(parent,
                tr("Unable to read the source file: the file is damaged or has an unknown format."));
        }
        return false;
    }

    if (!silent)
    {
        MsgBox message_box(MsgBox::Warning,
            tr("Warning"),
            tr("The existing settings will be overwritten. Continue?"),
            MsgBox::Yes | MsgBox::No,
            parent);

        if (message_box.exec() == MsgBox::No)
        {
            LOG(INFO) << "Import canceled by user";
            return false;
        }
    }

    QJsonObject root = doc.object();
    importSystemSettings(root.value(kSystem).toObject());

    if (!importDatabase(root.value(kDatabase).toObject()))
    {
        if (!silent)
            MsgBox::warning(parent, tr("Unable to write the secure database."));
        return false;
    }

    if (!silent)
        MsgBox::information(parent, tr("The configuration was successfully imported."));

    return true;
}

//--------------------------------------------------------------------------------------------------
// static
bool SettingsUtil::exportToFile(const QString& path, bool silent, QWidget* parent)
{
    LOG(INFO) << "Export settings to" << path;

    if (!Database::instance().isValid())
    {
        LOG(ERROR) << "Database is not valid";
        if (!silent)
            MsgBox::warning(parent, tr("Unable to read the secure database."));
        return false;
    }

    if (QFileInfo::exists(path) && !silent)
    {
        MsgBox message_box(MsgBox::Warning,
            tr("Warning"),
            tr("The existing settings will be overwritten. Continue?"),
            MsgBox::Yes | MsgBox::No,
            parent);

        if (message_box.exec() == MsgBox::No)
        {
            LOG(INFO) << "Export canceled by user";
            return false;
        }
    }

    QJsonObject root;
    root[kSystem] = exportSystemSettings();
    root[kDatabase] = exportDatabase();

    QFile file(path);
    if (!file.open(QFile::WriteOnly | QFile::Truncate))
    {
        LOG(ERROR) << "Unable to open target file:" << file.errorString();
        if (!silent)
            MsgBox::warning(parent, tr("Unable to open the target file."));
        return false;
    }

    if (file.write(QJsonDocument(root).toJson(QJsonDocument::Indented)) == -1)
    {
        LOG(ERROR) << "Failed to write target file:" << file.errorString();
        if (!silent)
            MsgBox::warning(parent, tr("Unable to write the target file."));
        return false;
    }

    if (!silent)
        MsgBox::information(parent, tr("The configuration was successfully exported."));

    return true;
}
