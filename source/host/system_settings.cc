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

#include "host/system_settings.h"

#include "base/xml_settings.h"
#include "base/crypto/password_generator.h"
#include "base/crypto/password_hash.h"
#include "base/crypto/random.h"
#include "base/peer/user_list.h"
#include "build/build_config.h"

namespace host {

namespace {

const size_t kPasswordHashSaltSize = 256;

} // namespace

//--------------------------------------------------------------------------------------------------
SystemSettings::SystemSettings()
    : settings_(base::XmlSettings::format(), QSettings::SystemScope, "aspia", "host")
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
SystemSettings::~SystemSettings() = default;

//--------------------------------------------------------------------------------------------------
// static
bool SystemSettings::createPasswordHash(const QString& password, QByteArray* hash, QByteArray* salt)
{
    if (password.isEmpty() || !hash || !salt)
        return false;

    QByteArray salt_temp = base::Random::byteArray(kPasswordHashSaltSize);
    if (salt_temp.isEmpty())
        return false;

    QByteArray hash_temp =
        base::PasswordHash::hash(base::PasswordHash::SCRYPT, password, salt_temp);
    if (hash_temp.isEmpty())
        return false;

    *salt = std::move(salt_temp);
    *hash = std::move(hash_temp);
    return true;
}

//--------------------------------------------------------------------------------------------------
// static
bool SystemSettings::isValidPassword(const QString& password)
{
    if (password.isEmpty())
        return false;

    SystemSettings settings;

    QByteArray password_hash_salt = settings.passwordHashSalt();
    QByteArray password_hash = settings.passwordHash();

    if (password_hash_salt.isEmpty() || password_hash.isEmpty())
        return false;

    QByteArray verifiable_password_hash =
        base::PasswordHash::hash(base::PasswordHash::SCRYPT, password, password_hash_salt);
    if (verifiable_password_hash.isEmpty())
        return false;

    return verifiable_password_hash == password_hash;
}

//--------------------------------------------------------------------------------------------------
QString SystemSettings::filePath() const
{
    return settings_.fileName();
}

//--------------------------------------------------------------------------------------------------
bool SystemSettings::isWritable() const
{
    return settings_.isWritable();
}

//--------------------------------------------------------------------------------------------------
void SystemSettings::sync()
{
    settings_.sync();
}

//--------------------------------------------------------------------------------------------------
quint16 SystemSettings::tcpPort() const
{
    return settings_.value("TcpPort", DEFAULT_HOST_TCP_PORT).toUInt();
}

//--------------------------------------------------------------------------------------------------
void SystemSettings::setTcpPort(quint16 port)
{
    settings_.setValue("TcpPort", port);
}

//--------------------------------------------------------------------------------------------------
bool SystemSettings::isRouterEnabled() const
{
    return settings_.value("RouterEnabled", false).toBool();
}

//--------------------------------------------------------------------------------------------------
void SystemSettings::setRouterEnabled(bool enable)
{
    settings_.setValue("RouterEnabled", enable);
}

//--------------------------------------------------------------------------------------------------
QString SystemSettings::routerAddress() const
{
    return settings_.value("RouterAddress").toString();
}

//--------------------------------------------------------------------------------------------------
void SystemSettings::setRouterAddress(const QString& address)
{
    settings_.setValue("RouterAddress", address);
}

//--------------------------------------------------------------------------------------------------
quint16 SystemSettings::routerPort() const
{
    return settings_.value("RouterPort", DEFAULT_ROUTER_TCP_PORT).toUInt();
}

//--------------------------------------------------------------------------------------------------
void SystemSettings::setRouterPort(quint16 port)
{
    settings_.setValue("RouterPort", port);
}

//--------------------------------------------------------------------------------------------------
QByteArray SystemSettings::routerPublicKey() const
{
    return settings_.value("RouterPublicKey").toByteArray();
}

//--------------------------------------------------------------------------------------------------
void SystemSettings::setRouterPublicKey(const QByteArray& key)
{
    settings_.setValue("RouterPublicKey", key);
}

//--------------------------------------------------------------------------------------------------
std::unique_ptr<base::UserList> SystemSettings::userList() const
{
    std::unique_ptr<base::UserList> users = base::UserList::createEmpty();

    int count = settings_.beginReadArray("Users");
    for (int i = 0; i < count; ++i)
    {
        settings_.setArrayIndex(i);

        base::User user;
        user.name     = settings_.value("Name").toString();
        user.group    = settings_.value("Group").toString();
        user.salt     = settings_.value("Salt").toByteArray();
        user.verifier = settings_.value("Verifier").toByteArray();
        user.sessions = settings_.value("Sessions").toUInt();
        user.flags    = settings_.value("Flags").toUInt();

        users->add(user);
    }
    settings_.endArray();

    QByteArray seed_key = settings_.value("SeedKey").toByteArray();
    if (seed_key.isEmpty())
        seed_key = base::Random::byteArray(64);

    users->setSeedKey(seed_key);
    return users;
}

//--------------------------------------------------------------------------------------------------
void SystemSettings::setUserList(const base::UserList& users)
{
    // Clear the old list of users.
    settings_.remove("Users");

    QVector<base::User> list = users.list();

    settings_.beginWriteArray("Users");
    for (int i = 0; i < list.size(); ++i)
    {
        settings_.setArrayIndex(i);

        const base::User& user = list.at(i);
        settings_.setValue("Name", user.name);
        settings_.setValue("Group", user.group);
        settings_.setValue("Salt", user.salt);
        settings_.setValue("Verifier", user.verifier);
        settings_.setValue("Sessions", user.sessions);
        settings_.setValue("Flags", user.flags);
    }
    settings_.endArray();

    QByteArray seed_key = users.seedKey();
    if (seed_key.isEmpty())
        seed_key = base::Random::byteArray(64);

    settings_.setValue("SeedKey", seed_key);
}

//--------------------------------------------------------------------------------------------------
QString SystemSettings::updateServer() const
{
    return settings_.value("UpdateServer", QString(DEFAULT_UPDATE_SERVER)).toString();
}

//--------------------------------------------------------------------------------------------------
void SystemSettings::setUpdateServer(const QString& server)
{
    settings_.setValue("UpdateServer", server);
}

//--------------------------------------------------------------------------------------------------
quint32 SystemSettings::preferredVideoCapturer() const
{
    return settings_.value("PreferredVideoCapturer", 0).toUInt();
}

//--------------------------------------------------------------------------------------------------
void SystemSettings::setPreferredVideoCapturer(quint32 type)
{
    settings_.setValue("PreferredVideoCapturer", type);
}

//--------------------------------------------------------------------------------------------------
bool SystemSettings::passwordProtection() const
{
    return settings_.value("PasswordProtection", false).toBool();
}

//--------------------------------------------------------------------------------------------------
void SystemSettings::setPasswordProtection(bool enable)
{
    settings_.setValue("PasswordProtection", enable);
}

//--------------------------------------------------------------------------------------------------
QByteArray SystemSettings::passwordHash() const
{
    return settings_.value("PasswordHash").toByteArray();
}

//--------------------------------------------------------------------------------------------------
void SystemSettings::setPasswordHash(const QByteArray& hash)
{
    settings_.setValue("PasswordHash", hash);
}

//--------------------------------------------------------------------------------------------------
QByteArray SystemSettings::passwordHashSalt() const
{
    return settings_.value("PasswordHashSalt").toByteArray();
}

//--------------------------------------------------------------------------------------------------
void SystemSettings::setPasswordHashSalt(const QByteArray& salt)
{
    settings_.setValue("PasswordHashSalt", salt);
}

//--------------------------------------------------------------------------------------------------
bool SystemSettings::oneTimePassword() const
{
    return settings_.value("OneTimePassword", true).toBool();
}

//--------------------------------------------------------------------------------------------------
void SystemSettings::setOneTimePassword(bool enable)
{
    settings_.setValue("OneTimePassword", enable);
}

//--------------------------------------------------------------------------------------------------
std::chrono::milliseconds SystemSettings::oneTimePasswordExpire() const
{
    static const std::chrono::milliseconds kDefaultValue { 5 * 60 * 1000 }; // 5 minutes.
    static const std::chrono::milliseconds kMinValue { 0 };
    static const std::chrono::milliseconds kMaxValue { 24 * 60 * 60 * 1000 }; // 24 hours.

    std::chrono::milliseconds value(
        settings_.value("OneTimePasswordExpire", kDefaultValue.count()).toLongLong());

    if (value < kMinValue)
        value = kMinValue;
    else if (value > kMaxValue)
        value = kMaxValue;

    return value;
}

//--------------------------------------------------------------------------------------------------
void SystemSettings::setOneTimePasswordExpire(const std::chrono::milliseconds& interval)
{
    settings_.setValue("OneTimePasswordExpire", interval.count());
}

//--------------------------------------------------------------------------------------------------
int SystemSettings::oneTimePasswordLength() const
{
    static const int kDefaultValue = 8;
    static const int kMinValue = 6;
    static const int kMaxValue = 16;

    int value = settings_.value("OneTimePasswordLength", kDefaultValue).toInt();

    if (value < kMinValue)
        value = kMinValue;
    else if (value > kMaxValue)
        value = kMaxValue;

    return value;
}

//--------------------------------------------------------------------------------------------------
void SystemSettings::setOneTimePasswordLength(int length)
{
    settings_.setValue("OneTimePasswordLength", length);
}

//--------------------------------------------------------------------------------------------------
quint32 SystemSettings::oneTimePasswordCharacters() const
{
    quint32 kDefaultValue = base::PasswordGenerator::DIGITS | base::PasswordGenerator::LOWER_CASE |
        base::PasswordGenerator::UPPER_CASE;

    quint32 value = settings_.value("OneTimePasswordCharacters", kDefaultValue).toUInt();

    if (!(value & base::PasswordGenerator::DIGITS) &&
        !(value & base::PasswordGenerator::LOWER_CASE) &&
        !(value & base::PasswordGenerator::UPPER_CASE))
    {
        value = kDefaultValue;
    }

    return value;
}

//--------------------------------------------------------------------------------------------------
void SystemSettings::setOneTimePasswordCharacters(quint32 characters)
{
    settings_.setValue("OneTimePasswordCharacters", characters);
}

//--------------------------------------------------------------------------------------------------
bool SystemSettings::connConfirm() const
{
    return settings_.value("ConnConfirm", false).toBool();
}

//--------------------------------------------------------------------------------------------------
void SystemSettings::setConnConfirm(bool enable)
{
    settings_.setValue("ConnConfirm", enable);
}

//--------------------------------------------------------------------------------------------------
SystemSettings::NoUserAction SystemSettings::connConfirmNoUserAction() const
{
    return static_cast<NoUserAction>(
        settings_.value("ConnConfirmNoUserAction", static_cast<int>(NoUserAction::ACCEPT)).toInt());
}

//--------------------------------------------------------------------------------------------------
void SystemSettings::setConnConfirmNoUserAction(NoUserAction action)
{
    settings_.setValue("ConnConfirmNoUserAction", static_cast<int>(action));
}

//--------------------------------------------------------------------------------------------------
std::chrono::milliseconds SystemSettings::autoConnConfirmInterval() const
{
    static const std::chrono::milliseconds kDefaultValue { 0 };
    static const std::chrono::milliseconds kMinValue { 0 };
    static const std::chrono::milliseconds kMaxValue { 60 * 1000 }; // 60 seconds.

    std::chrono::milliseconds value(
        settings_.value("AutoConnConfirmInterval", kDefaultValue.count()).toLongLong());

    if (value < kMinValue)
        value = kMinValue;
    else if (value > kMaxValue)
        value = kMaxValue;

    return value;
}

//--------------------------------------------------------------------------------------------------
void SystemSettings::setAutoConnConfirmInterval(const std::chrono::milliseconds& interval)
{
    settings_.setValue("AutoConnConfirmInterval", interval.count());
}

//--------------------------------------------------------------------------------------------------
bool SystemSettings::isApplicationShutdownDisabled() const
{
    return settings_.value("ApplicationShutdownDisabled", false).toBool();
}

//--------------------------------------------------------------------------------------------------
void SystemSettings::setApplicationShutdownDisabled(bool value)
{
    settings_.setValue("ApplicationShutdownDisabled", value);
}

//--------------------------------------------------------------------------------------------------
bool SystemSettings::isAutoUpdateEnabled() const
{
    return settings_.value("AutoUpdateEnabled", true).toBool();
}

//--------------------------------------------------------------------------------------------------
void SystemSettings::setAutoUpdateEnabled(bool enable)
{
    settings_.setValue("AutoUpdateEnabled", enable);
}

//--------------------------------------------------------------------------------------------------
int SystemSettings::updateCheckFrequency() const
{
    return settings_.value("UpdateCheckFrequency", 7).toInt();
}

//--------------------------------------------------------------------------------------------------
void SystemSettings::setUpdateCheckFrequency(int days)
{
    settings_.setValue("UpdateCheckFrequency", days);
}

} // namespace host
