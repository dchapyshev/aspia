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
#include "base/crypto/password_hash.h"
#include "base/crypto/random.h"
#include "base/crypto/secure_string.h"
#include "base/files/base_paths.h"

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
QByteArray Database::passwordHash() const
{
    return QByteArray::fromHex(readSetting(kSettingPasswordHash).toLatin1());
}

//--------------------------------------------------------------------------------------------------
bool Database::setPasswordHash(const QByteArray& hash)
{
    return writeSetting(kSettingPasswordHash, QString::fromLatin1(hash.toHex()));
}

//--------------------------------------------------------------------------------------------------
QByteArray Database::passwordHashSalt() const
{
    return QByteArray::fromHex(readSetting(kSettingPasswordHashSalt).toLatin1());
}

//--------------------------------------------------------------------------------------------------
bool Database::setPasswordHashSalt(const QByteArray& salt)
{
    return writeSetting(kSettingPasswordHashSalt, QString::fromLatin1(salt.toHex()));
}

//--------------------------------------------------------------------------------------------------
// static
bool Database::createPasswordHash(const SecureString& password, QByteArray* hash, QByteArray* salt)
{
    if (password.isEmpty() || !hash || !salt)
        return false;

    QByteArray salt_temp = Random::byteArray(kPasswordHashSaltSize);
    if (salt_temp.isEmpty())
        return false;

    QByteArray hash_temp = PasswordHash::hash(PasswordHash::SCRYPT, password, salt_temp);
    if (hash_temp.isEmpty())
        return false;

    *salt = std::move(salt_temp);
    *hash = std::move(hash_temp);
    return true;
}

//--------------------------------------------------------------------------------------------------
// static
bool Database::isValidPassword(const SecureString& password)
{
    if (password.isEmpty())
        return false;

    QByteArray password_hash_salt = instance().passwordHashSalt();
    QByteArray password_hash = instance().passwordHash();

    if (password_hash_salt.isEmpty() || password_hash.isEmpty())
        return false;

    QByteArray verifiable_password_hash =
        PasswordHash::hash(PasswordHash::SCRYPT, password, password_hash_salt);
    if (verifiable_password_hash.isEmpty())
        return false;

    return verifiable_password_hash == password_hash;
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
