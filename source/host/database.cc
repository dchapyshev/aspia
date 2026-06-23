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

#include "host/database.h"

#include <QDir>
#include <QFileInfo>

#include "base/logging.h"
#include "base/crypto/password_generator.h"
#include "base/crypto/password_hash.h"
#include "base/crypto/random.h"
#include "base/crypto/secure_string.h"
#include "base/files/base_paths.h"
#include "base/sql/sql_query.h"
#include "build/build_config.h"

namespace {

const char kSettingSeedKey[] = "seed_key";
const char kSettingTcpPort[] = "tcp_port";
const char kSettingRouterEnabled[] = "router_enabled";
const char kSettingRouterAddress[] = "router_address";
const char kSettingRouterPublicKey[] = "router_public_key";
const char kSettingConnectConfirmation[] = "connect_confirmation";
const char kSettingNoUserAction[] = "no_user_action";
const char kSettingAutoConfirmationInterval[] = "auto_confirmation_interval";
const char kSettingOneTimePassword[] = "one_time_password";
const char kSettingOneTimePasswordExpire[] = "one_time_password_expire";
const char kSettingOneTimePasswordLength[] = "one_time_password_length";
const char kSettingOneTimePasswordCharacters[] = "one_time_password_characters";
const char kSettingPasswordHash[] = "password_hash";
const char kSettingPasswordHashSalt[] = "password_hash_salt";

constexpr size_t kPasswordHashSaltSize = 256;

//--------------------------------------------------------------------------------------------------
User readUser(const SqlQuery& query)
{
    User user;
    user.entry_id = query.columnInt64(0);
    user.name     = query.columnText(1);
    user.group    = query.columnText(2);
    user.salt     = query.columnBlob(3);
    user.verifier = query.columnBlob(4);
    user.sessions = static_cast<quint32>(query.columnInt64(5));
    user.flags    = static_cast<quint32>(query.columnInt64(6));
    return user;
}

//--------------------------------------------------------------------------------------------------
bool createTables(SqlDatabase& db)
{
    if (!db.exec("CREATE TABLE IF NOT EXISTS \"users\" ("
                 "\"id\" INTEGER UNIQUE,"
                 "\"name\" TEXT NOT NULL UNIQUE,"
                 "\"group\" TEXT NOT NULL,"
                 "\"salt\" BLOB NOT NULL,"
                 "\"verifier\" BLOB NOT NULL,"
                 "\"sessions\" INTEGER DEFAULT 0,"
                 "\"flags\" INTEGER DEFAULT 0,"
                 "PRIMARY KEY(\"id\" AUTOINCREMENT))"))
    {
        LOG(ERROR) << "Unable to create users table:" << db.lastError();
        return false;
    }

    if (!db.exec("CREATE TABLE IF NOT EXISTS \"settings\" ("
                 "\"name\" TEXT PRIMARY KEY NOT NULL,"
                 "\"value\" TEXT NOT NULL)"))
    {
        LOG(ERROR) << "Unable to create settings table:" << db.lastError();
        return false;
    }

    return true;
}

} // namespace

//--------------------------------------------------------------------------------------------------
// static
Database& Database::instance()
{
    static thread_local Database database;

    if (!database.db_.isOpen())
        database.openDatabase();

    return database;
}

//--------------------------------------------------------------------------------------------------
// static
QString Database::directoryPath()
{
    QString dir_path = BasePaths::appConfigDir();
    if (dir_path.isEmpty())
        return QString();

    return dir_path + "/secure";
}

//--------------------------------------------------------------------------------------------------
// static
QString Database::filePath()
{
    QString dir_path = directoryPath();
    if (dir_path.isEmpty())
        return QString();

    return dir_path + "/host.db3";
}

//--------------------------------------------------------------------------------------------------
bool Database::isValid() const
{
    return db_.isOpen();
}

//--------------------------------------------------------------------------------------------------
QVector<User> Database::userList() const
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return {};
    }

    SqlQuery query(db_, "SELECT id, name, \"group\", salt, verifier, sessions, flags FROM users");

    QVector<User> users;
    while (query.next())
        users.append(readUser(query));

    return users;
}

//--------------------------------------------------------------------------------------------------
User Database::findUser(const QString& username) const
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return User::kInvalidUser;
    }

    SqlQuery query(db_,
        "SELECT id, name, \"group\", salt, verifier, sessions, flags FROM users WHERE name=?");
    query.addText(username);

    if (!query.next())
        return User::kInvalidUser;

    return readUser(query);
}

//--------------------------------------------------------------------------------------------------
User Database::findUser(qint64 entry_id) const
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return User::kInvalidUser;
    }

    SqlQuery query(db_,
        "SELECT id, name, \"group\", salt, verifier, sessions, flags FROM users WHERE id=?");
    query.addInt64(entry_id);

    if (!query.next())
        return User::kInvalidUser;

    return readUser(query);
}

//--------------------------------------------------------------------------------------------------
bool Database::addUser(const User& user)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    if (!user.isValid())
    {
        LOG(ERROR) << "Not valid user";
        return false;
    }

    SqlQuery query(db_, "INSERT INTO users (id, name, \"group\", salt, verifier, sessions, flags) "
                        "VALUES (NULL, ?, ?, ?, ?, ?, ?)");
    query.addText(user.name);
    query.addText(user.group);
    query.addBlob(user.salt);
    query.addBlob(user.verifier);
    query.addUInt64(user.sessions);
    query.addUInt64(user.flags);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool Database::modifyUser(const User& user)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    if (!user.isValid())
    {
        LOG(ERROR) << "Not valid user";
        return false;
    }

    SqlQuery query(db_, "UPDATE users SET name=?, \"group\"=?, salt=?, verifier=?, sessions=?, "
                        "flags=? WHERE id=?");
    query.addText(user.name);
    query.addText(user.group);
    query.addBlob(user.salt);
    query.addBlob(user.verifier);
    query.addUInt64(user.sessions);
    query.addUInt64(user.flags);
    query.addInt64(user.entry_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool Database::removeUser(qint64 entry_id)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    SqlQuery query(db_, "DELETE FROM users WHERE id=?");
    query.addInt64(entry_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
QByteArray Database::seedKey() const
{
    return QByteArray::fromHex(readSetting(kSettingSeedKey).toLatin1());
}

//--------------------------------------------------------------------------------------------------
bool Database::setSeedKey(const QByteArray& seed_key)
{
    return writeSetting(kSettingSeedKey, QString::fromLatin1(seed_key.toHex()));
}

//--------------------------------------------------------------------------------------------------
quint16 Database::tcpPort() const
{
    bool ok = false;
    uint value = readSetting(kSettingTcpPort).toUInt(&ok);
    if (!ok)
        return DEFAULT_HOST_TCP_PORT;
    return static_cast<quint16>(value);
}

//--------------------------------------------------------------------------------------------------
bool Database::setTcpPort(quint16 port)
{
    return writeSetting(kSettingTcpPort, QString::number(port));
}

//--------------------------------------------------------------------------------------------------
bool Database::isRouterEnabled() const
{
    return readSetting(kSettingRouterEnabled).toInt() != 0;
}

//--------------------------------------------------------------------------------------------------
bool Database::setRouterEnabled(bool enable)
{
    return writeSetting(kSettingRouterEnabled, QString::number(enable ? 1 : 0));
}

//--------------------------------------------------------------------------------------------------
Address Database::routerAddress() const
{
    return Address::fromString(readSetting(kSettingRouterAddress), DEFAULT_ROUTER_TCP_PORT);
}

//--------------------------------------------------------------------------------------------------
bool Database::setRouterAddress(const Address& address)
{
    return writeSetting(kSettingRouterAddress, address.toString());
}

//--------------------------------------------------------------------------------------------------
QByteArray Database::routerPublicKey() const
{
    return QByteArray::fromHex(readSetting(kSettingRouterPublicKey).toLatin1());
}

//--------------------------------------------------------------------------------------------------
bool Database::setRouterPublicKey(const QByteArray& key)
{
    return writeSetting(kSettingRouterPublicKey, QString::fromLatin1(key.toHex()));
}

//--------------------------------------------------------------------------------------------------
bool Database::connectConfirmation() const
{
    return readSetting(kSettingConnectConfirmation).toInt() != 0;
}

//--------------------------------------------------------------------------------------------------
bool Database::setConnectConfirmation(bool enable)
{
    return writeSetting(kSettingConnectConfirmation, QString::number(enable ? 1 : 0));
}

//--------------------------------------------------------------------------------------------------
Database::NoUserAction Database::noUserAction() const
{
    bool ok = false;
    int value = readSetting(kSettingNoUserAction).toInt(&ok);
    if (!ok)
        return NoUserAction::ACCEPT;
    return static_cast<NoUserAction>(value);
}

//--------------------------------------------------------------------------------------------------
bool Database::setNoUserAction(NoUserAction action)
{
    return writeSetting(kSettingNoUserAction, QString::number(static_cast<int>(action)));
}

//--------------------------------------------------------------------------------------------------
std::chrono::milliseconds Database::autoConfirmationInterval() const
{
    static const std::chrono::milliseconds kDefaultValue { 0 };
    static const std::chrono::milliseconds kMinValue { 0 };
    static const std::chrono::milliseconds kMaxValue { 60 * 1000 }; // 60 seconds.

    bool ok = false;
    qint64 value = readSetting(kSettingAutoConfirmationInterval).toLongLong(&ok);
    if (!ok)
        return kDefaultValue;

    std::chrono::milliseconds result(value);
    if (result < kMinValue)
        result = kMinValue;
    else if (result > kMaxValue)
        result = kMaxValue;

    return result;
}

//--------------------------------------------------------------------------------------------------
bool Database::setAutoConfirmationInterval(const std::chrono::milliseconds& interval)
{
    return writeSetting(kSettingAutoConfirmationInterval,
                        QString::number(static_cast<qint64>(interval.count())));
}

//--------------------------------------------------------------------------------------------------
bool Database::oneTimePassword() const
{
    QString value = readSetting(kSettingOneTimePassword);
    if (value.isEmpty())
        return true;
    return value.toInt() != 0;
}

//--------------------------------------------------------------------------------------------------
bool Database::setOneTimePassword(bool enable)
{
    return writeSetting(kSettingOneTimePassword, QString::number(enable ? 1 : 0));
}

//--------------------------------------------------------------------------------------------------
std::chrono::milliseconds Database::oneTimePasswordExpire() const
{
    static const std::chrono::milliseconds kDefaultValue { 5 * 60 * 1000 }; // 5 minutes.
    static const std::chrono::milliseconds kMinValue { 0 };
    static const std::chrono::milliseconds kMaxValue { 24 * 60 * 60 * 1000 }; // 24 hours.

    bool ok = false;
    qint64 value = readSetting(kSettingOneTimePasswordExpire).toLongLong(&ok);
    if (!ok)
        return kDefaultValue;

    std::chrono::milliseconds result(value);
    if (result < kMinValue)
        result = kMinValue;
    else if (result > kMaxValue)
        result = kMaxValue;

    return result;
}

//--------------------------------------------------------------------------------------------------
bool Database::setOneTimePasswordExpire(const std::chrono::milliseconds& interval)
{
    return writeSetting(kSettingOneTimePasswordExpire,
                        QString::number(static_cast<qint64>(interval.count())));
}

//--------------------------------------------------------------------------------------------------
int Database::oneTimePasswordLength() const
{
    static const int kDefaultValue = 8;
    static const int kMinValue = 6;
    static const int kMaxValue = 16;

    bool ok = false;
    int value = readSetting(kSettingOneTimePasswordLength).toInt(&ok);
    if (!ok)
        return kDefaultValue;

    if (value < kMinValue)
        value = kMinValue;
    else if (value > kMaxValue)
        value = kMaxValue;

    return value;
}

//--------------------------------------------------------------------------------------------------
bool Database::setOneTimePasswordLength(int length)
{
    return writeSetting(kSettingOneTimePasswordLength, QString::number(length));
}

//--------------------------------------------------------------------------------------------------
quint32 Database::oneTimePasswordCharacters() const
{
    const quint32 kDefaultValue = PasswordGenerator::DIGITS | PasswordGenerator::LOWER_CASE |
        PasswordGenerator::UPPER_CASE;

    bool ok = false;
    quint32 value = readSetting(kSettingOneTimePasswordCharacters).toUInt(&ok);
    if (!ok)
        return kDefaultValue;

    if (!(value & PasswordGenerator::DIGITS) &&
        !(value & PasswordGenerator::LOWER_CASE) &&
        !(value & PasswordGenerator::UPPER_CASE))
    {
        value = kDefaultValue;
    }

    return value;
}

//--------------------------------------------------------------------------------------------------
bool Database::setOneTimePasswordCharacters(quint32 characters)
{
    return writeSetting(kSettingOneTimePasswordCharacters, QString::number(characters));
}

//--------------------------------------------------------------------------------------------------
Database::PasswordProtection Database::passwordProtectionState() const
{
    if (!isValid())
        return PasswordProtection::UNAVAILABLE;

    return (!passwordHash().isEmpty() && !passwordHashSalt().isEmpty()) ?
        PasswordProtection::ENABLED : PasswordProtection::DISABLED;
}

//--------------------------------------------------------------------------------------------------
bool Database::setPassword(const SecureString& password)
{
    if (password.isEmpty())
        return false;

    QByteArray salt = Random::byteArray(kPasswordHashSaltSize);
    if (salt.isEmpty())
        return false;

    QByteArray hash = PasswordHash::hash(PasswordHash::SCRYPT, password, salt);
    if (hash.isEmpty())
        return false;

    return writeSetting(kSettingPasswordHash, QString::fromLatin1(hash.toHex())) &&
           writeSetting(kSettingPasswordHashSalt, QString::fromLatin1(salt.toHex()));
}

//--------------------------------------------------------------------------------------------------
void Database::clearPassword()
{
    writeSetting(kSettingPasswordHash, QString());
    writeSetting(kSettingPasswordHashSalt, QString());
}

//--------------------------------------------------------------------------------------------------
bool Database::verifyPassword(const SecureString& password) const
{
    if (password.isEmpty())
        return false;

    QByteArray salt = passwordHashSalt();
    QByteArray hash = passwordHash();

    if (salt.isEmpty() || hash.isEmpty())
        return false;

    QByteArray verifiable_hash = PasswordHash::hash(PasswordHash::SCRYPT, password, salt);
    if (verifiable_hash.isEmpty())
        return false;

    return verifiable_hash == hash;
}

//--------------------------------------------------------------------------------------------------
bool Database::openDatabase()
{
    QString dir_path = directoryPath();
    if (dir_path.isEmpty())
    {
        LOG(ERROR) << "Invalid directory path";
        return false;
    }

    QFileInfo dir_info(dir_path);
    if (dir_info.exists())
    {
        if (!dir_info.isDir())
        {
            LOG(ERROR) << "Unable to create directory for database. Need to delete file:"
                       << dir_path;
            return false;
        }
    }
    else
    {
        if (!QDir().mkpath(dir_path))
        {
            LOG(ERROR) << "Unable to create directory for database";
            return false;
        }
    }

    QString file_path = filePath();
    if (file_path.isEmpty())
    {
        LOG(ERROR) << "Invalid file path";
        return false;
    }

    LOG(INFO) << (!QFileInfo::exists(file_path) ? "Creating" : "Opening") << "database:"
              << file_path;

    if (!db_.open(file_path))
        return false;

    // The configuration is shared with the elevated settings editor (a separate process). Wait out
    // its brief write locks instead of failing reads with "database is locked". Otherwise a settings
    // change racing with this read is misread (e.g. router treated as disabled).
    if (!db_.setBusyTimeout(5000))
        LOG(WARNING) << "Unable to set busy timeout:" << db_.lastError();

    if (!db_.exec("PRAGMA secure_delete = ON"))
        LOG(WARNING) << "Unable to enable secure_delete:" << db_.lastError();

    {
        SqlQuery pragma(db_, "PRAGMA quick_check");
        if (pragma.next())
        {
            const QString result = pragma.columnText(0);
            if (result != "ok")
                LOG(ERROR) << "Database integrity check failed:" << result;
        }
        else
        {
            LOG(WARNING) << "Unable to run quick_check:" << db_.lastError();
        }
    }

    if (!createTables(db_))
    {
        db_.close();
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
QString Database::readSetting(const QString& name) const
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return QString();
    }

    SqlQuery query(db_, "SELECT value FROM settings WHERE name=?");
    query.addText(name);

    if (!query.next())
        return QString();

    return query.columnText(0);
}

//--------------------------------------------------------------------------------------------------
bool Database::writeSetting(const QString& name, const QString& value)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    SqlQuery query(db_, "INSERT OR REPLACE INTO settings (name, value) VALUES (?, ?)");
    query.addText(name);
    query.addText(value);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
QByteArray Database::passwordHash() const
{
    return QByteArray::fromHex(readSetting(kSettingPasswordHash).toLatin1());
}

//--------------------------------------------------------------------------------------------------
QByteArray Database::passwordHashSalt() const
{
    return QByteArray::fromHex(readSetting(kSettingPasswordHashSalt).toLatin1());
}
