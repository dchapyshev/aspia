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
#include <QFile>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

#include "base/logging.h"
#include "base/crypto/password_generator.h"
#include "base/crypto/password_hash.h"
#include "base/crypto/random.h"
#include "base/crypto/secure_string.h"
#include "base/files/base_paths.h"
#include "build/build_config.h"

#if defined(Q_OS_WINDOWS)
#include "base/win/security_helpers.h"
#include <qt_windows.h>
#include <aclapi.h>
#endif

#if defined(Q_OS_UNIX)
#include <sys/stat.h>
#endif

namespace {

const char kConnectionName[] = "host";
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

#if defined(Q_OS_WINDOWS)
//--------------------------------------------------------------------------------------------------
bool applyPathSecurity(const QString& path, bool /* is_dir */)
{
    // Owner: SYSTEM. Protected DACL: SYSTEM and BUILTIN\Administrators have Full Control. Inherited
    // by child containers and objects. Setting an explicit owner closes the implicit READ_CONTROL /
    // WRITE_DAC rights that the previous owner (potentially a regular user who created the
    // directory before the service started) would otherwise retain. Non-elevated administrator
    // processes do not pass this DACL because in their tokens the Administrators SID is deny-only.
    ScopedSd sd = convertSddlToSd("O:SYG:SYD:P(A;OICI;GA;;;SY)(A;OICI;GA;;;BA)");
    if (!sd)
    {
        LOG(ERROR) << "convertSddlToSd failed";
        return false;
    }

    BOOL present = FALSE;
    BOOL defaulted = FALSE;
    PACL dacl = nullptr;
    if (!GetSecurityDescriptorDacl(sd.get(), &present, &dacl, &defaulted) || !present)
    {
        PLOG(ERROR) << "GetSecurityDescriptorDacl failed";
        return false;
    }

    PSID owner = nullptr;
    if (!GetSecurityDescriptorOwner(sd.get(), &owner, &defaulted) || !owner)
    {
        PLOG(ERROR) << "GetSecurityDescriptorOwner failed";
        return false;
    }

    DWORD result = SetNamedSecurityInfoW(const_cast<wchar_t*>(qUtf16Printable(path)), SE_FILE_OBJECT,
        OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION,
        owner, nullptr, dacl, nullptr);

    if (result != ERROR_SUCCESS)
    {
        LOG(ERROR) << "SetNamedSecurityInfoW failed for" << path << "error:" << result;
        return false;
    }

    return true;
}
#endif // defined(Q_OS_WINDOWS)

#if defined(Q_OS_UNIX)
//--------------------------------------------------------------------------------------------------
bool applyPathSecurity(const QString& path, bool is_dir)
{
    const QByteArray native = QFile::encodeName(path);

    // Reset the owner to root (uid 0, gid 0). If the entry was pre-created by a non-root user,
    // chmod alone is not enough - the user remains the owner and would retain access. Service
    // must run as root for this call to succeed.
    if (chown(native.constData(), 0, 0) != 0)
    {
        PLOG(ERROR) << "chown failed for" << path;
        return false;
    }

    const mode_t mode = is_dir ? S_IRWXU : (S_IRUSR | S_IWUSR);
    if (chmod(native.constData(), mode) != 0)
    {
        PLOG(ERROR) << "chmod failed for" << path;
        return false;
    }

    return true;
}
#endif // defined(Q_OS_UNIX)

//--------------------------------------------------------------------------------------------------
User readUser(const QSqlQuery& query)
{
    User user;
    user.entry_id = query.value(0).toLongLong();
    user.name = query.value(1).toString();
    user.group = query.value(2).toString();
    user.salt = query.value(3).toByteArray();
    user.verifier = query.value(4).toByteArray();
    user.sessions = query.value(5).toUInt();
    user.flags = query.value(6).toUInt();
    return user;
}

//--------------------------------------------------------------------------------------------------
bool createTables(QSqlDatabase& db)
{
    QSqlQuery query(db);

    if (!query.exec("CREATE TABLE IF NOT EXISTS \"users\" ("
                    "\"id\" INTEGER UNIQUE,"
                    "\"name\" TEXT NOT NULL UNIQUE,"
                    "\"group\" TEXT NOT NULL,"
                    "\"salt\" BLOB NOT NULL,"
                    "\"verifier\" BLOB NOT NULL,"
                    "\"sessions\" INTEGER DEFAULT 0,"
                    "\"flags\" INTEGER DEFAULT 0,"
                    "PRIMARY KEY(\"id\" AUTOINCREMENT))"))
    {
        LOG(ERROR) << "Unable to create users table:" << query.lastError();
        return false;
    }

    if (!query.exec("CREATE TABLE IF NOT EXISTS \"settings\" ("
                    "\"name\" TEXT PRIMARY KEY NOT NULL,"
                    "\"value\" TEXT NOT NULL)"))
    {
        LOG(ERROR) << "Unable to create settings table:" << query.lastError();
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

    if (!database.valid_)
        database.valid_ = database.openDatabase();

    return database;
}

//--------------------------------------------------------------------------------------------------
// static
QString Database::directoryPath()
{
    QString dir_path = BasePaths::appConfigDir();
    if (dir_path.isEmpty())
        return QString();

    return dir_path + "/db";
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
// static
bool Database::applyPermissions()
{
    QString dir_path = directoryPath();
    if (dir_path.isEmpty())
    {
        LOG(ERROR) << "Invalid directory path";
        return false;
    }

    if (!QDir().mkpath(dir_path))
    {
        LOG(ERROR) << "Unable to create directory:" << dir_path;
        return false;
    }

    bool ok = applyPathSecurity(dir_path, true);

    QDir dir(dir_path);
    const QFileInfoList entries =
        dir.entryInfoList(QDir::Files | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot);
    for (const QFileInfo& entry : entries)
    {
        if (!applyPathSecurity(entry.absoluteFilePath(), false))
            ok = false;
    }

    return ok;
}

//--------------------------------------------------------------------------------------------------
bool Database::isValid() const
{
    return valid_;
}

//--------------------------------------------------------------------------------------------------
QVector<User> Database::userList() const
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return {};
    }

    QSqlQuery query(QSqlDatabase::database(kConnectionName, false));
    if (!query.exec("SELECT id, name, \"group\", salt, verifier, sessions, flags FROM users"))
    {
        LOG(ERROR) << "Unable to get user list:" << query.lastError();
        return {};
    }

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

    QSqlQuery query(QSqlDatabase::database(kConnectionName, false));
    query.prepare(
        "SELECT id, name, \"group\", salt, verifier, sessions, flags FROM users WHERE name=?");
    query.addBindValue(username);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << query.lastError();
        return User::kInvalidUser;
    }

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

    QSqlQuery query(QSqlDatabase::database(kConnectionName, false));
    query.prepare(
        "SELECT id, name, \"group\", salt, verifier, sessions, flags FROM users WHERE id=?");
    query.addBindValue(entry_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << query.lastError();
        return User::kInvalidUser;
    }

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

    QSqlQuery query(QSqlDatabase::database(kConnectionName, false));
    query.prepare("INSERT INTO users (id, name, \"group\", salt, verifier, sessions, flags) "
                  "VALUES (NULL, ?, ?, ?, ?, ?, ?)");
    query.addBindValue(user.name);
    query.addBindValue(user.group);
    query.addBindValue(user.salt);
    query.addBindValue(user.verifier);
    query.addBindValue(static_cast<qulonglong>(user.sessions));
    query.addBindValue(static_cast<qulonglong>(user.flags));

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << query.lastError();
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

    QSqlQuery query(QSqlDatabase::database(kConnectionName, false));
    query.prepare("UPDATE users SET name=?, \"group\"=?, salt=?, verifier=?, sessions=?, flags=? "
                  "WHERE id=?");
    query.addBindValue(user.name);
    query.addBindValue(user.group);
    query.addBindValue(user.salt);
    query.addBindValue(user.verifier);
    query.addBindValue(static_cast<qulonglong>(user.sessions));
    query.addBindValue(static_cast<qulonglong>(user.flags));
    query.addBindValue(user.entry_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << query.lastError();
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

    QSqlQuery query(QSqlDatabase::database(kConnectionName, false));
    query.prepare("DELETE FROM users WHERE id=?");
    query.addBindValue(entry_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << query.lastError();
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

    QSqlDatabase db = QSqlDatabase::database(kConnectionName, false);
    if (!db.isValid())
    {
        db = QSqlDatabase::addDatabase("QSQLITE", kConnectionName);
        db.setDatabaseName(file_path);
    }

    if (!db.isOpen() && !db.open())
    {
        LOG(ERROR) << "QSqlDatabase::open failed:" << db.lastError();
        return false;
    }

    if (!createTables(db))
    {
        db.close();
        QSqlDatabase::removeDatabase(kConnectionName);
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

    QSqlQuery query(QSqlDatabase::database(kConnectionName, false));
    query.prepare("SELECT value FROM settings WHERE name=?");
    query.addBindValue(name);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << query.lastError();
        return QString();
    }

    if (!query.next())
        return QString();

    return query.value(0).toString();
}

//--------------------------------------------------------------------------------------------------
bool Database::writeSetting(const QString& name, const QString& value)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    QSqlQuery query(QSqlDatabase::database(kConnectionName, false));
    query.prepare("INSERT OR REPLACE INTO settings (name, value) VALUES (?, ?)");
    query.addBindValue(name);
    query.addBindValue(value);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << query.lastError();
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
