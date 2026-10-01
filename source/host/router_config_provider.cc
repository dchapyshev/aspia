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

#include "host/router_config_provider.h"

#include "host/database.h"
#include "host/host_user_list.h"

#if defined(Q_OS_WINDOWS)
#include "base/crypto/password_generator.h"
#include "base/crypto/random.h"
#endif // defined(Q_OS_WINDOWS)

namespace {

#if defined(Q_OS_WINDOWS)

const MilliSeconds kOneTimePasswordExpire { 5 * 60 * 1000 }; // 5 minutes.
const int kSeedKeySize = 64;

class PortableUserList final : public UserList
{
public:
    PortableUserList()
        : seed_key_(Random::byteArray(kSeedKeySize))
    {
        // Nothing
    }

    // UserList implementation.
    User find(const QString& username) const final
    {
        if (one_time_user_.isValid() &&
            one_time_user_.name.compare(username, Qt::CaseInsensitive) == 0)
        {
            return one_time_user_;
        }

        return User();
    }

    QByteArray seedKey() const final { return seed_key_; }
    void setSeedKey(const QByteArray& seed_key) final { seed_key_ = seed_key; }
    void setOneTimeUser(const User& user) final { one_time_user_ = user; }

private:
    QByteArray seed_key_;
    User one_time_user_;
};

#endif // defined(Q_OS_WINDOWS)

} // namespace

//--------------------------------------------------------------------------------------------------
DatabaseConfigProvider::DatabaseConfigProvider(Database& database)
    : database_(database)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
DatabaseConfigProvider::~DatabaseConfigProvider() = default;

//--------------------------------------------------------------------------------------------------
bool DatabaseConfigProvider::isPortable() const
{
    return false;
}

//--------------------------------------------------------------------------------------------------
Address DatabaseConfigProvider::routerAddress() const
{
    return database_.routerAddress();
}

//--------------------------------------------------------------------------------------------------
QByteArray DatabaseConfigProvider::routerPublicKey() const
{
    return database_.routerPublicKey();
}

//--------------------------------------------------------------------------------------------------
bool DatabaseConfigProvider::oneTimePassword() const
{
    return database_.oneTimePassword();
}

//--------------------------------------------------------------------------------------------------
quint32 DatabaseConfigProvider::oneTimePasswordCharacters() const
{
    return database_.oneTimePasswordCharacters();
}

//--------------------------------------------------------------------------------------------------
int DatabaseConfigProvider::oneTimePasswordLength() const
{
    return database_.oneTimePasswordLength();
}

//--------------------------------------------------------------------------------------------------
MilliSeconds DatabaseConfigProvider::oneTimePasswordExpire() const
{
    return database_.oneTimePasswordExpire();
}

//--------------------------------------------------------------------------------------------------
QByteArray DatabaseConfigProvider::hostKey() const
{
    return database_.hostKey();
}

//--------------------------------------------------------------------------------------------------
bool DatabaseConfigProvider::setHostKey(const QByteArray& key)
{
    return database_.setHostKey(key);
}

//--------------------------------------------------------------------------------------------------
QVector<User> DatabaseConfigProvider::userList() const
{
    return database_.userList();
}

//--------------------------------------------------------------------------------------------------
Database::PasswordProtection DatabaseConfigProvider::passwordProtectionState() const
{
    return database_.passwordProtectionState();
}

//--------------------------------------------------------------------------------------------------
SharedPointer<UserList> DatabaseConfigProvider::createUserList() const
{
    return SharedPointer<UserList>(new HostUserList(database_));
}

#if defined(Q_OS_WINDOWS)
//--------------------------------------------------------------------------------------------------
PortableConfigProvider::PortableConfigProvider()
    : settings_(PortablePackage::builtInSettings().value_or(PortablePackage::Settings()))
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
PortableConfigProvider::~PortableConfigProvider() = default;

//--------------------------------------------------------------------------------------------------
bool PortableConfigProvider::isPortable() const
{
    return true;
}

//--------------------------------------------------------------------------------------------------
Address PortableConfigProvider::routerAddress() const
{
    return settings_.router_address;
}

//--------------------------------------------------------------------------------------------------
QByteArray PortableConfigProvider::routerPublicKey() const
{
    return settings_.router_public_key;
}

//--------------------------------------------------------------------------------------------------
bool PortableConfigProvider::oneTimePassword() const
{
    return true;
}

//--------------------------------------------------------------------------------------------------
quint32 PortableConfigProvider::oneTimePasswordCharacters() const
{
    return PasswordGenerator::DIGITS;
}

//--------------------------------------------------------------------------------------------------
int PortableConfigProvider::oneTimePasswordLength() const
{
    return 9;
}

//--------------------------------------------------------------------------------------------------
MilliSeconds PortableConfigProvider::oneTimePasswordExpire() const
{
    return kOneTimePasswordExpire;
}

//--------------------------------------------------------------------------------------------------
QByteArray PortableConfigProvider::hostKey() const
{
    // The portable host is never registered: it always asks for a temporary id.
    return QByteArray();
}

//--------------------------------------------------------------------------------------------------
bool PortableConfigProvider::setHostKey(const QByteArray& /* key */)
{
    // The temporary key is never stored.
    return true;
}

//--------------------------------------------------------------------------------------------------
QVector<User> PortableConfigProvider::userList() const
{
    return QVector<User>();
}

//--------------------------------------------------------------------------------------------------
Database::PasswordProtection PortableConfigProvider::passwordProtectionState() const
{
    return Database::PasswordProtection::DISABLED;
}

//--------------------------------------------------------------------------------------------------
SharedPointer<UserList> PortableConfigProvider::createUserList() const
{
    return SharedPointer<UserList>(new PortableUserList());
}
#endif // defined(Q_OS_WINDOWS)
