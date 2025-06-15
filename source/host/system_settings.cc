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

const QString kOrganization = QStringLiteral("aspia");
const QString kApplication = QStringLiteral("host");

const QString kTcpPort = QStringLiteral("tcp_port");
const QString kApplicationShutdown = QStringLiteral("application_shutdown");
const QString kPreferredVideoCapturer = QStringLiteral("preferred_video_capturer");

const QString kRouterEnable = QStringLiteral("router/enable");
const QString kRouterAddress = QStringLiteral("router/address");
const QString kRouterPort = QStringLiteral("router/port");
const QString kRouterPublicKey = QStringLiteral("router/public_key");

const QString kSeedKey = QStringLiteral("seed_key");
const QString kUsers = QStringLiteral("users");
const QString kUserName = QStringLiteral("name");
const QString kUserGroup = QStringLiteral("group");
const QString kUserSalt = QStringLiteral("salt");
const QString kUserVerifier = QStringLiteral("verifier");
const QString kUserSessions = QStringLiteral("sessions");
const QString kUserFlags = QStringLiteral("flags");

const QString kUpdateServer = QStringLiteral("update/server");
const QString kUpdateAutoUpdate = QStringLiteral("update/auto_update");
const QString kUpdateCheckFrequency = QStringLiteral("update/check_frequency");

const QString kPasswordProtectionEnable = QStringLiteral("password_protection/enable");
const QString kPasswordProtectionHash = QStringLiteral("password_protection/hash");
const QString kPasswordProtectionSalt = QStringLiteral("password_protection/salt");

const QString kOneTimePasswordEnable = QStringLiteral("one_time_password/enable");
const QString kOneTimePasswordExpire = QStringLiteral("one_time_password/expire");
const QString kOneTimePasswordLength = QStringLiteral("one_time_password/length");
const QString kOneTimePasswordCharacters = QStringLiteral("one_time_password/characters");

const QString kConnectConfirmationEnable = QStringLiteral("connect_confirmation/enable");
const QString kConnectConfirmationNoUserAction =
    QStringLiteral("connect_confirmation/no_user_action");
const QString kConnectConfirmationAutoConfirmationInterval =
    QStringLiteral("connect_confirmation/auto_confirmation_interval");

} // namespace

//--------------------------------------------------------------------------------------------------
SystemSettings::SystemSettings()
    : settings_(base::XmlSettings::format(), QSettings::SystemScope, kOrganization, kApplication)
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

    QByteArray hash_temp = base::PasswordHash::hash(base::PasswordHash::SCRYPT, password, salt_temp);
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
    return settings_.value(kTcpPort, DEFAULT_HOST_TCP_PORT).toUInt();
}

//--------------------------------------------------------------------------------------------------
void SystemSettings::setTcpPort(quint16 port)
{
    settings_.setValue(kTcpPort, port);
}

//--------------------------------------------------------------------------------------------------
bool SystemSettings::isRouterEnabled() const
{
    return settings_.value(kRouterEnable, false).toBool();
}

//--------------------------------------------------------------------------------------------------
void SystemSettings::setRouterEnabled(bool enable)
{
    settings_.setValue(kRouterEnable, enable);
}

//--------------------------------------------------------------------------------------------------
QString SystemSettings::routerAddress() const
{
    return settings_.value(kRouterAddress).toString();
}

//--------------------------------------------------------------------------------------------------
void SystemSettings::setRouterAddress(const QString& address)
{
    settings_.setValue(kRouterAddress, address);
}

//--------------------------------------------------------------------------------------------------
quint16 SystemSettings::routerPort() const
{
    return settings_.value(kRouterPort, DEFAULT_ROUTER_TCP_PORT).toUInt();
}

//--------------------------------------------------------------------------------------------------
void SystemSettings::setRouterPort(quint16 port)
{
    settings_.setValue(kRouterPort, port);
}

//--------------------------------------------------------------------------------------------------
QByteArray SystemSettings::routerPublicKey() const
{
    return settings_.value(kRouterPublicKey).toByteArray();
}

//--------------------------------------------------------------------------------------------------
void SystemSettings::setRouterPublicKey(const QByteArray& key)
{
    settings_.setValue(kRouterPublicKey, key);
}

//--------------------------------------------------------------------------------------------------
std::unique_ptr<base::UserList> SystemSettings::userList() const
{
    std::unique_ptr<base::UserList> users = base::UserList::createEmpty();

    int count = settings_.beginReadArray(kUsers);
    for (int i = 0; i < count; ++i)
    {
        settings_.setArrayIndex(i);

        base::User user;
        user.name     = settings_.value(kUserName).toString();
        user.group    = settings_.value(kUserGroup).toString();
        user.salt     = settings_.value(kUserSalt).toByteArray();
        user.verifier = settings_.value(kUserVerifier).toByteArray();
        user.sessions = settings_.value(kUserSessions).toUInt();
        user.flags    = settings_.value(kUserFlags).toUInt();

        users->add(user);
    }
    settings_.endArray();

    QByteArray seed_key = settings_.value(kSeedKey).toByteArray();
    if (seed_key.isEmpty())
        seed_key = base::Random::byteArray(64);

    users->setSeedKey(seed_key);
    return users;
}

//--------------------------------------------------------------------------------------------------
void SystemSettings::setUserList(const base::UserList& users)
{
    // Clear the old list of users.
    settings_.remove(kUsers);

    QVector<base::User> list = users.list();

    settings_.beginWriteArray(kUsers);
    for (int i = 0; i < list.size(); ++i)
    {
        settings_.setArrayIndex(i);

        const base::User& user = list.at(i);
        settings_.setValue(kUserName, user.name);
        settings_.setValue(kUserGroup, user.group);
        settings_.setValue(kUserSalt, user.salt);
        settings_.setValue(kUserVerifier, user.verifier);
        settings_.setValue(kUserSessions, user.sessions);
        settings_.setValue(kUserFlags, user.flags);
    }
    settings_.endArray();

    QByteArray seed_key = users.seedKey();
    if (seed_key.isEmpty())
        seed_key = base::Random::byteArray(64);

    settings_.setValue(kSeedKey, seed_key);
}

//--------------------------------------------------------------------------------------------------
QString SystemSettings::updateServer() const
{
    return settings_.value(kUpdateServer, QString(DEFAULT_UPDATE_SERVER)).toString();
}

//--------------------------------------------------------------------------------------------------
void SystemSettings::setUpdateServer(const QString& server)
{
    settings_.setValue(kUpdateServer, server);
}

//--------------------------------------------------------------------------------------------------
quint32 SystemSettings::preferredVideoCapturer() const
{
    return settings_.value(kPreferredVideoCapturer, 0).toUInt();
}

//--------------------------------------------------------------------------------------------------
void SystemSettings::setPreferredVideoCapturer(quint32 type)
{
    settings_.setValue(kPreferredVideoCapturer, type);
}

//--------------------------------------------------------------------------------------------------
bool SystemSettings::passwordProtection() const
{
    return settings_.value(kPasswordProtectionEnable, false).toBool();
}

//--------------------------------------------------------------------------------------------------
void SystemSettings::setPasswordProtection(bool enable)
{
    settings_.setValue(kPasswordProtectionEnable, enable);
}

//--------------------------------------------------------------------------------------------------
QByteArray SystemSettings::passwordHash() const
{
    return settings_.value(kPasswordProtectionHash).toByteArray();
}

//--------------------------------------------------------------------------------------------------
void SystemSettings::setPasswordHash(const QByteArray& hash)
{
    settings_.setValue(kPasswordProtectionHash, hash);
}

//--------------------------------------------------------------------------------------------------
QByteArray SystemSettings::passwordHashSalt() const
{
    return settings_.value(kPasswordProtectionSalt).toByteArray();
}

//--------------------------------------------------------------------------------------------------
void SystemSettings::setPasswordHashSalt(const QByteArray& salt)
{
    settings_.setValue(kPasswordProtectionSalt, salt);
}

//--------------------------------------------------------------------------------------------------
bool SystemSettings::oneTimePassword() const
{
    return settings_.value(kOneTimePasswordEnable, true).toBool();
}

//--------------------------------------------------------------------------------------------------
void SystemSettings::setOneTimePassword(bool enable)
{
    settings_.setValue(kOneTimePasswordEnable, enable);
}

//--------------------------------------------------------------------------------------------------
std::chrono::milliseconds SystemSettings::oneTimePasswordExpire() const
{
    static const std::chrono::milliseconds kDefaultValue { 5 * 60 * 1000 }; // 5 minutes.
    static const std::chrono::milliseconds kMinValue { 0 };
    static const std::chrono::milliseconds kMaxValue { 24 * 60 * 60 * 1000 }; // 24 hours.

    std::chrono::milliseconds value(
        settings_.value(kOneTimePasswordExpire, kDefaultValue.count()).toLongLong());

    if (value < kMinValue)
        value = kMinValue;
    else if (value > kMaxValue)
        value = kMaxValue;

    return value;
}

//--------------------------------------------------------------------------------------------------
void SystemSettings::setOneTimePasswordExpire(const std::chrono::milliseconds& interval)
{
    settings_.setValue(kOneTimePasswordExpire, interval.count());
}

//--------------------------------------------------------------------------------------------------
int SystemSettings::oneTimePasswordLength() const
{
    static const int kDefaultValue = 8;
    static const int kMinValue = 6;
    static const int kMaxValue = 16;

    int value = settings_.value(kOneTimePasswordLength, kDefaultValue).toInt();

    if (value < kMinValue)
        value = kMinValue;
    else if (value > kMaxValue)
        value = kMaxValue;

    return value;
}

//--------------------------------------------------------------------------------------------------
void SystemSettings::setOneTimePasswordLength(int length)
{
    settings_.setValue(kOneTimePasswordLength, length);
}

//--------------------------------------------------------------------------------------------------
quint32 SystemSettings::oneTimePasswordCharacters() const
{
    quint32 kDefaultValue = base::PasswordGenerator::DIGITS | base::PasswordGenerator::LOWER_CASE |
        base::PasswordGenerator::UPPER_CASE;

    quint32 value = settings_.value(kOneTimePasswordCharacters, kDefaultValue).toUInt();

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
    settings_.setValue(kOneTimePasswordCharacters, characters);
}

//--------------------------------------------------------------------------------------------------
bool SystemSettings::connectConfirmation() const
{
    return settings_.value(kConnectConfirmationEnable, false).toBool();
}

//--------------------------------------------------------------------------------------------------
void SystemSettings::setConnectConfirmation(bool enable)
{
    settings_.setValue(kConnectConfirmationEnable, enable);
}

//--------------------------------------------------------------------------------------------------
SystemSettings::NoUserAction SystemSettings::noUserAction() const
{
    return static_cast<NoUserAction>(settings_.value(
        kConnectConfirmationNoUserAction, static_cast<int>(NoUserAction::ACCEPT)).toInt());
}

//--------------------------------------------------------------------------------------------------
void SystemSettings::setNoUserAction(NoUserAction action)
{
    settings_.setValue(kConnectConfirmationNoUserAction, static_cast<int>(action));
}

//--------------------------------------------------------------------------------------------------
std::chrono::milliseconds SystemSettings::autoConfirmationInterval() const
{
    static const std::chrono::milliseconds kDefaultValue { 0 };
    static const std::chrono::milliseconds kMinValue { 0 };
    static const std::chrono::milliseconds kMaxValue { 60 * 1000 }; // 60 seconds.

    std::chrono::milliseconds value(settings_.value(
        kConnectConfirmationAutoConfirmationInterval, kDefaultValue.count()).toLongLong());

    if (value < kMinValue)
        value = kMinValue;
    else if (value > kMaxValue)
        value = kMaxValue;

    return value;
}

//--------------------------------------------------------------------------------------------------
void SystemSettings::setAutoConfirmationInterval(const std::chrono::milliseconds& interval)
{
    settings_.setValue(kConnectConfirmationAutoConfirmationInterval, interval.count());
}

//--------------------------------------------------------------------------------------------------
bool SystemSettings::isApplicationShutdownDisabled() const
{
    return settings_.value(kApplicationShutdown, false).toBool();
}

//--------------------------------------------------------------------------------------------------
void SystemSettings::setApplicationShutdownDisabled(bool value)
{
    settings_.setValue(kApplicationShutdown, value);
}

//--------------------------------------------------------------------------------------------------
bool SystemSettings::isAutoUpdateEnabled() const
{
    return settings_.value(kUpdateAutoUpdate, true).toBool();
}

//--------------------------------------------------------------------------------------------------
void SystemSettings::setAutoUpdateEnabled(bool enable)
{
    settings_.setValue(kUpdateAutoUpdate, enable);
}

//--------------------------------------------------------------------------------------------------
int SystemSettings::updateCheckFrequency() const
{
    return settings_.value(kUpdateCheckFrequency, 7).toInt();
}

//--------------------------------------------------------------------------------------------------
void SystemSettings::setUpdateCheckFrequency(int days)
{
    settings_.setValue(kUpdateCheckFrequency, days);
}

} // namespace host
