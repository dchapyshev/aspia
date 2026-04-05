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

#include "client/book_database.h"

#include "base/crypto/data_cryptor_chacha20_poly1305.h"
#include "base/crypto/password_hash.h"
#include "base/crypto/random.h"
#include "base/crypto/secure_memory.h"
#include "base/logging.h"

#include <QDir>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QVariant>

#include <atomic>
#include <utility>

namespace client {

namespace {

std::atomic<int> g_connection_counter{ 0 };

//--------------------------------------------------------------------------------------------------
QString nextConnectionName()
{
    return QString("book_%1").arg(g_connection_counter.fetch_add(1));
}

//--------------------------------------------------------------------------------------------------
QSqlDatabase databaseByName(const QString& connection_name)
{
    if (connection_name.isEmpty())
        return QSqlDatabase();

    return QSqlDatabase::database(connection_name, false);
}

//--------------------------------------------------------------------------------------------------
QSqlDatabase ensureOpenDatabase(const QString& file_path, const QString& connection_name)
{
    QSqlDatabase db = databaseByName(connection_name);
    if (db.isValid())
    {
        if (!db.isOpen() && !db.open())
        {
            LOG(ERROR) << "QSqlDatabase::open failed:" << db.lastError();
            return QSqlDatabase();
        }

        return db;
    }

    db = QSqlDatabase::addDatabase("QSQLITE", connection_name);
    db.setDatabaseName(file_path);

    if (!db.open())
    {
        LOG(ERROR) << "QSqlDatabase::open failed:" << db.lastError();
        db = QSqlDatabase();
        QSqlDatabase::removeDatabase(connection_name);
        return QSqlDatabase();
    }

    return db;
}

//--------------------------------------------------------------------------------------------------
QByteArray encryptBlob(const QByteArray& key, const QByteArray& data)
{
    if (key.isEmpty())
        return data;

    base::DataCryptorChaCha20Poly1305 cryptor(key);
    QByteArray out;
    if (!cryptor.encrypt(data, &out))
    {
        LOG(ERROR) << "Failed to encrypt data";
        return QByteArray();
    }
    return out;
}

//--------------------------------------------------------------------------------------------------
QByteArray decryptBlob(const QByteArray& key, const QByteArray& data)
{
    if (key.isEmpty())
        return data;

    if (data.isEmpty())
        return data;

    base::DataCryptorChaCha20Poly1305 cryptor(key);
    QByteArray out;
    if (!cryptor.decrypt(data, &out))
    {
        LOG(ERROR) << "Failed to decrypt data";
        return QByteArray();
    }
    return out;
}

//--------------------------------------------------------------------------------------------------
ComputerData readComputer(const QSqlQuery& query, const QByteArray& key)
{
    ComputerData computer;
    computer.id = query.value(0).toLongLong();
    computer.group_id = query.value(1).toLongLong();
    computer.name = query.value(2).toString();
    computer.comment = QString::fromUtf8(decryptBlob(key, query.value(3).toByteArray()));
    computer.address = QString::fromUtf8(decryptBlob(key, query.value(4).toByteArray()));
    computer.username = QString::fromUtf8(decryptBlob(key, query.value(5).toByteArray()));
    computer.password = QString::fromUtf8(decryptBlob(key, query.value(6).toByteArray()));
    return computer;
}

//--------------------------------------------------------------------------------------------------
ComputerGroupData readGroup(const QSqlQuery& query, const QByteArray& key)
{
    ComputerGroupData group;
    group.id = query.value(0).toLongLong();
    group.parent_id = query.value(1).toLongLong();
    group.name = query.value(2).toString();
    group.comment = QString::fromUtf8(decryptBlob(key, query.value(3).toByteArray()));
    group.expanded = query.value(4).toBool();
    return group;
}

//--------------------------------------------------------------------------------------------------
QByteArray deriveKey(const QString& password, const QByteArray& salt)
{
    return base::PasswordHash::hash(base::PasswordHash::SCRYPT, password, salt);
}

const char kKeyEncryptionType[] = "encryption_type";
const char kKeyHashingSalt[] = "hashing_salt";
const char kKeyRouterEnabled[] = "router_enabled";
const char kKeyRouterAddress[] = "router_address";
const char kKeyRouterPort[] = "router_port";
const char kKeyRouterUsername[] = "router_username";
const char kKeyRouterPassword[] = "router_password";
const char kKeyDisplayName[] = "display_name";

} // namespace

//--------------------------------------------------------------------------------------------------
BookDatabase::BookDatabase() = default;

//--------------------------------------------------------------------------------------------------
BookDatabase::BookDatabase(const QString& connection_name, const QByteArray& encryption_key)
    : encryption_key_(encryption_key),
      connection_name_(connection_name)
{
    DCHECK(!connection_name_.isEmpty());
}

//--------------------------------------------------------------------------------------------------
BookDatabase::BookDatabase(BookDatabase&& other) noexcept
    : encryption_key_(std::move(other.encryption_key_)),
      connection_name_(std::move(other.connection_name_))
{
    other.connection_name_.clear();
    other.encryption_key_.clear();
}

//--------------------------------------------------------------------------------------------------
BookDatabase& BookDatabase::operator=(BookDatabase&& other) noexcept
{
    if (this != &other)
    {
        base::memZero(&encryption_key_);

        encryption_key_ = std::move(other.encryption_key_);
        connection_name_ = std::move(other.connection_name_);
    }

    other.connection_name_.clear();
    other.encryption_key_.clear();
    return *this;
}

//--------------------------------------------------------------------------------------------------
BookDatabase::~BookDatabase()
{
    base::memZero(&encryption_key_);
}

//--------------------------------------------------------------------------------------------------
// static
BookDatabase BookDatabase::create(EncryptionType encryption_type, const QString& password)
{
    QString dir_path = databaseDirectory();
    if (dir_path.isEmpty())
    {
        LOG(ERROR) << "Invalid directory path";
        return BookDatabase();
    }

    QFileInfo dir_info(dir_path);
    if (dir_info.exists())
    {
        if (!dir_info.isDir())
        {
            LOG(ERROR) << "Unable to create directory for database. Need to delete file:" << dir_path;
            return BookDatabase();
        }
    }
    else
    {
        if (!QDir().mkpath(dir_path))
        {
            LOG(ERROR) << "Unable to create directory for database";
            return BookDatabase();
        }
    }

    QString file_path = filePath();
    if (file_path.isEmpty())
    {
        LOG(ERROR) << "Invalid file path";
        return BookDatabase();
    }

    if (QFileInfo::exists(file_path))
    {
        LOG(ERROR) << "Database file already exists";
        return BookDatabase();
    }

    QByteArray encryption_key;
    QByteArray salt;

    if (encryption_type == EncryptionType::CHACHA20_POLY1305)
    {
        if (password.isEmpty())
        {
            LOG(ERROR) << "Password is required for encrypted database";
            return BookDatabase();
        }

        salt = base::Random::byteArray(base::PasswordHash::kBytesSize);
        encryption_key = deriveKey(password, salt);
    }

    QString connection_name = nextConnectionName();
    QSqlDatabase db = ensureOpenDatabase(file_path, connection_name);
    if (!db.isValid() || !db.isOpen())
        return BookDatabase();

    if (!db.transaction())
    {
        LOG(ERROR) << "Unable to execute transaction:" << db.lastError();
        return BookDatabase();
    }

    bool success = false;

    do
    {
        QSqlQuery query(db);

        if (!query.exec("CREATE TABLE IF NOT EXISTS \"groups\" ("
                        "\"id\" INTEGER UNIQUE,"
                        "\"parent_id\" INTEGER NOT NULL DEFAULT 0,"
                        "\"name\" TEXT NOT NULL DEFAULT '',"
                        "\"comment\" BLOB NOT NULL DEFAULT X'',"
                        "\"expanded\" INTEGER NOT NULL DEFAULT 0,"
                        "PRIMARY KEY(\"id\" AUTOINCREMENT))"))
        {
            LOG(ERROR) << "Unable to execute query:" << query.lastError();
            break;
        }

        if (!query.exec("CREATE TABLE IF NOT EXISTS \"computers\" ("
                        "\"id\" INTEGER UNIQUE,"
                        "\"group_id\" INTEGER NOT NULL DEFAULT 0,"
                        "\"name\" TEXT NOT NULL DEFAULT '',"
                        "\"comment\" BLOB NOT NULL DEFAULT X'',"
                        "\"address\" BLOB NOT NULL DEFAULT X'',"
                        "\"username\" BLOB NOT NULL DEFAULT X'',"
                        "\"password\" BLOB NOT NULL DEFAULT X'',"
                        "PRIMARY KEY(\"id\" AUTOINCREMENT),"
                        "FOREIGN KEY(\"group_id\") REFERENCES \"groups\"(\"id\") ON DELETE CASCADE)"))
        {
            LOG(ERROR) << "Unable to execute query:" << query.lastError();
            break;
        }

        if (!query.exec("CREATE TABLE IF NOT EXISTS \"config\" ("
                        "\"key\" TEXT NOT NULL UNIQUE,"
                        "\"value\" BLOB NOT NULL DEFAULT X'',"
                        "PRIMARY KEY(\"key\"))"))
        {
            LOG(ERROR) << "Unable to execute query:" << query.lastError();
            break;
        }

        if (!query.exec("PRAGMA foreign_keys = ON"))
        {
            LOG(ERROR) << "Unable to enable foreign keys:" << query.lastError();
            break;
        }

        // Store encryption type.
        query.prepare("INSERT INTO config (key, value) VALUES (?, ?)");
        query.addBindValue(kKeyEncryptionType);
        query.addBindValue(QByteArray::number(static_cast<int>(encryption_type)));
        if (!query.exec())
        {
            LOG(ERROR) << "Unable to execute query:" << query.lastError();
            break;
        }

        // Store hashing salt.
        query.prepare("INSERT INTO config (key, value) VALUES (?, ?)");
        query.addBindValue(kKeyHashingSalt);
        query.addBindValue(salt);
        if (!query.exec())
        {
            LOG(ERROR) << "Unable to execute query:" << query.lastError();
            break;
        }

        success = true;
    }
    while (false);

    if (!success)
    {
        db.rollback();
        return BookDatabase();
    }

    if (!db.commit())
    {
        LOG(ERROR) << "Unable to commit transaction:" << db.lastError();
        db.rollback();
        return BookDatabase();
    }

    return BookDatabase(connection_name, encryption_key);
}

//--------------------------------------------------------------------------------------------------
// static
BookDatabase BookDatabase::open(const QString& password)
{
    QString file_path = filePath();
    if (file_path.isEmpty())
    {
        LOG(ERROR) << "Invalid file path";
        return BookDatabase();
    }

    if (!QFileInfo::exists(file_path))
    {
        LOG(ERROR) << "Database file does not exist:" << file_path;
        return BookDatabase();
    }

    LOG(INFO) << "Opening book database:" << file_path;

    QString connection_name = nextConnectionName();
    QSqlDatabase db = ensureOpenDatabase(file_path, connection_name);
    if (!db.isValid() || !db.isOpen())
        return BookDatabase();

    // Enable foreign keys.
    {
        QSqlQuery query(db);
        if (!query.exec("PRAGMA foreign_keys = ON"))
            LOG(WARNING) << "Unable to enable foreign keys:" << query.lastError();
    }

    // Read encryption type.
    QByteArray encryption_key;
    {
        QSqlQuery query(db);
        query.prepare("SELECT value FROM config WHERE key=?");
        query.addBindValue(kKeyEncryptionType);

        if (!query.exec() || !query.next())
        {
            LOG(ERROR) << "Unable to read encryption type:" << query.lastError();
            return BookDatabase();
        }

        int type = query.value(0).toByteArray().toInt();
        if (type == static_cast<int>(EncryptionType::CHACHA20_POLY1305))
        {
            if (password.isEmpty())
            {
                LOG(ERROR) << "Password is required for encrypted database";
                return BookDatabase();
            }

            // Read salt.
            query.prepare("SELECT value FROM config WHERE key=?");
            query.addBindValue(kKeyHashingSalt);

            if (!query.exec() || !query.next())
            {
                LOG(ERROR) << "Unable to read hashing salt:" << query.lastError();
                return BookDatabase();
            }

            QByteArray salt = query.value(0).toByteArray();
            encryption_key = deriveKey(password, salt);
        }
    }

    return BookDatabase(connection_name, encryption_key);
}

//--------------------------------------------------------------------------------------------------
// static
QString BookDatabase::filePath()
{
    QString dir_path = databaseDirectory();
    if (dir_path.isEmpty())
        return QString();

    return dir_path + "/book.db3";
}

//--------------------------------------------------------------------------------------------------
// static
bool BookDatabase::isEncrypted()
{
    QString file_path = filePath();
    if (file_path.isEmpty() || !QFileInfo::exists(file_path))
        return false;

    QString connection_name = nextConnectionName();
    QSqlDatabase db = ensureOpenDatabase(file_path, connection_name);
    if (!db.isValid() || !db.isOpen())
        return false;

    QSqlQuery query(db);
    query.prepare("SELECT value FROM config WHERE key=?");
    query.addBindValue(kKeyEncryptionType);

    bool encrypted = false;
    if (query.exec() && query.next())
    {
        int type = query.value(0).toByteArray().toInt();
        encrypted = (type != static_cast<int>(EncryptionType::NONE));
    }

    db.close();
    QSqlDatabase::removeDatabase(connection_name);
    return encrypted;
}

//--------------------------------------------------------------------------------------------------
bool BookDatabase::isValid() const
{
    const QSqlDatabase db = databaseByName(connection_name_);
    return db.isValid() && db.isOpen();
}

//--------------------------------------------------------------------------------------------------
QList<ComputerData> BookDatabase::computerList(qint64 group_id) const
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return {};
    }

    QSqlQuery query(databaseByName(connection_name_));
    query.prepare("SELECT id, group_id, name, comment, address, username, password "
                  "FROM computers WHERE group_id=?");
    query.addBindValue(group_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to get computer list:" << query.lastError();
        return {};
    }

    QList<ComputerData> computers;
    while (query.next())
        computers.append(readComputer(query, encryption_key_));

    return computers;
}

//--------------------------------------------------------------------------------------------------
bool BookDatabase::addComputer(ComputerData& computer)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    QSqlQuery query(databaseByName(connection_name_));
    query.prepare("INSERT INTO computers (id, group_id, name, comment, address, username, password) "
                  "VALUES (NULL, ?, ?, ?, ?, ?, ?)");
    query.addBindValue(computer.group_id);
    query.addBindValue(computer.name);
    query.addBindValue(encryptData(computer.comment.toUtf8()));
    query.addBindValue(encryptData(computer.address.toUtf8()));
    query.addBindValue(encryptData(computer.username.toUtf8()));
    query.addBindValue(encryptData(computer.password.toUtf8()));

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << query.lastError();
        return false;
    }

    computer.id = query.lastInsertId().toLongLong();
    return true;
}

//--------------------------------------------------------------------------------------------------
bool BookDatabase::modifyComputer(const ComputerData& computer)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    QSqlQuery query(databaseByName(connection_name_));
    query.prepare("UPDATE computers SET group_id=?, name=?, comment=?, address=?, username=?, password=? "
                  "WHERE id=?");
    query.addBindValue(computer.group_id);
    query.addBindValue(computer.name);
    query.addBindValue(encryptData(computer.comment.toUtf8()));
    query.addBindValue(encryptData(computer.address.toUtf8()));
    query.addBindValue(encryptData(computer.username.toUtf8()));
    query.addBindValue(encryptData(computer.password.toUtf8()));
    query.addBindValue(computer.id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << query.lastError();
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool BookDatabase::removeComputer(qint64 computer_id)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    QSqlQuery query(databaseByName(connection_name_));
    query.prepare("DELETE FROM computers WHERE id=?");
    query.addBindValue(computer_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << query.lastError();
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
std::optional<ComputerData> BookDatabase::findComputer(qint64 computer_id) const
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return std::nullopt;
    }

    QSqlQuery query(databaseByName(connection_name_));
    query.prepare("SELECT id, group_id, name, comment, address, username, password "
                  "FROM computers WHERE id=?");
    query.addBindValue(computer_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << query.lastError();
        return std::nullopt;
    }

    if (!query.next())
        return std::nullopt;

    return readComputer(query, encryption_key_);
}

//--------------------------------------------------------------------------------------------------
QList<ComputerData> BookDatabase::searchComputers(const QString& query_text) const
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return {};
    }

    // For encrypted databases, we can only search by name (which is stored as plaintext).
    // For unencrypted databases, we can search by name, address, and comment.
    QSqlQuery query(databaseByName(connection_name_));

    if (encryption_key_.isEmpty())
    {
        query.prepare("SELECT id, group_id, name, comment, address, username, password "
                      "FROM computers WHERE name LIKE ? OR address LIKE ? OR comment LIKE ?");

        QString pattern = QString("%%1%").arg(query_text);
        query.addBindValue(pattern);
        query.addBindValue(pattern);
        query.addBindValue(pattern);
    }
    else
    {
        query.prepare("SELECT id, group_id, name, comment, address, username, password "
                      "FROM computers WHERE name LIKE ?");

        query.addBindValue(QString("%%1%").arg(query_text));
    }

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute search query:" << query.lastError();
        return {};
    }

    QList<ComputerData> computers;
    while (query.next())
        computers.append(readComputer(query, encryption_key_));

    return computers;
}

//--------------------------------------------------------------------------------------------------
QList<ComputerGroupData> BookDatabase::groupList(qint64 parent_id) const
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return {};
    }

    QSqlQuery query(databaseByName(connection_name_));
    query.prepare("SELECT id, parent_id, name, comment, expanded FROM groups WHERE parent_id=?");
    query.addBindValue(parent_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to get group list:" << query.lastError();
        return {};
    }

    QList<ComputerGroupData> groups;
    while (query.next())
        groups.append(readGroup(query, encryption_key_));

    return groups;
}

//--------------------------------------------------------------------------------------------------
QList<ComputerGroupData> BookDatabase::allGroups() const
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return {};
    }

    QSqlQuery query(databaseByName(connection_name_));
    if (!query.exec("SELECT id, parent_id, name, comment, expanded FROM groups"))
    {
        LOG(ERROR) << "Unable to get all groups:" << query.lastError();
        return {};
    }

    QList<ComputerGroupData> groups;
    while (query.next())
        groups.append(readGroup(query, encryption_key_));

    return groups;
}

//--------------------------------------------------------------------------------------------------
bool BookDatabase::addGroup(ComputerGroupData& group)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    QSqlQuery query(databaseByName(connection_name_));
    query.prepare("INSERT INTO groups (id, parent_id, name, comment, expanded) "
                  "VALUES (NULL, ?, ?, ?, ?)");
    query.addBindValue(group.parent_id);
    query.addBindValue(group.name);
    query.addBindValue(encryptData(group.comment.toUtf8()));
    query.addBindValue(group.expanded ? 1 : 0);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << query.lastError();
        return false;
    }

    group.id = query.lastInsertId().toLongLong();
    return true;
}

//--------------------------------------------------------------------------------------------------
bool BookDatabase::modifyGroup(const ComputerGroupData& group)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    QSqlQuery query(databaseByName(connection_name_));
    query.prepare("UPDATE groups SET parent_id=?, name=?, comment=?, expanded=? WHERE id=?");
    query.addBindValue(group.parent_id);
    query.addBindValue(group.name);
    query.addBindValue(encryptData(group.comment.toUtf8()));
    query.addBindValue(group.expanded ? 1 : 0);
    query.addBindValue(group.id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << query.lastError();
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool BookDatabase::removeGroup(qint64 group_id)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    QSqlQuery query(databaseByName(connection_name_));
    query.prepare("DELETE FROM groups WHERE id=?");
    query.addBindValue(group_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << query.lastError();
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
std::optional<ComputerGroupData> BookDatabase::findGroup(qint64 group_id) const
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return std::nullopt;
    }

    QSqlQuery query(databaseByName(connection_name_));
    query.prepare("SELECT id, parent_id, name, comment, expanded FROM groups WHERE id=?");
    query.addBindValue(group_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << query.lastError();
        return std::nullopt;
    }

    if (!query.next())
        return std::nullopt;

    return readGroup(query, encryption_key_);
}

//--------------------------------------------------------------------------------------------------
BookDatabase::EncryptionType BookDatabase::encryptionType() const
{
    QByteArray value = configValue(kKeyEncryptionType);
    int type = value.toInt();

    if (type == static_cast<int>(EncryptionType::CHACHA20_POLY1305))
        return EncryptionType::CHACHA20_POLY1305;

    return EncryptionType::NONE;
}

//--------------------------------------------------------------------------------------------------
bool BookDatabase::setEncryption(EncryptionType encryption_type, const QString& password)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    QByteArray old_key = encryption_key_;
    QByteArray new_key;
    QByteArray new_salt;

    if (encryption_type == EncryptionType::CHACHA20_POLY1305)
    {
        if (password.isEmpty())
        {
            LOG(ERROR) << "Password is required for encrypted database";
            return false;
        }

        new_salt = base::Random::byteArray(base::PasswordHash::kBytesSize);
        new_key = deriveKey(password, new_salt);
    }

    if (!reencryptAll(old_key, new_key))
    {
        LOG(ERROR) << "Failed to re-encrypt data";
        return false;
    }

    encryption_key_ = new_key;

    setConfigValue(kKeyEncryptionType, QByteArray::number(static_cast<int>(encryption_type)));
    setConfigValue(kKeyHashingSalt, new_salt);

    base::memZero(&old_key);
    return true;
}

//--------------------------------------------------------------------------------------------------
bool BookDatabase::isRouterEnabled() const
{
    QByteArray value = configValue(kKeyRouterEnabled);
    return value.toInt() != 0;
}

//--------------------------------------------------------------------------------------------------
void BookDatabase::setRouterEnabled(bool enabled)
{
    setConfigValue(kKeyRouterEnabled, QByteArray::number(enabled ? 1 : 0));
}

//--------------------------------------------------------------------------------------------------
RouterConfig BookDatabase::routerConfig() const
{
    RouterConfig config;
    config.address = QString::fromUtf8(decryptData(configValue(kKeyRouterAddress)));
    config.port = static_cast<quint16>(configValue(kKeyRouterPort).toUInt());
    config.username = QString::fromUtf8(decryptData(configValue(kKeyRouterUsername)));
    config.password = QString::fromUtf8(decryptData(configValue(kKeyRouterPassword)));
    return config;
}

//--------------------------------------------------------------------------------------------------
void BookDatabase::setRouterConfig(const RouterConfig& config)
{
    setConfigValue(kKeyRouterAddress, encryptData(config.address.toUtf8()));
    setConfigValue(kKeyRouterPort, QByteArray::number(config.port));
    setConfigValue(kKeyRouterUsername, encryptData(config.username.toUtf8()));
    setConfigValue(kKeyRouterPassword, encryptData(config.password.toUtf8()));
}

//--------------------------------------------------------------------------------------------------
QString BookDatabase::displayName() const
{
    return QString::fromUtf8(configValue(kKeyDisplayName));
}

//--------------------------------------------------------------------------------------------------
void BookDatabase::setDisplayName(const QString& name)
{
    setConfigValue(kKeyDisplayName, name.toUtf8());
}

//--------------------------------------------------------------------------------------------------
// static
QString BookDatabase::databaseDirectory()
{
    QString dir_path = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (dir_path.isEmpty())
    {
        LOG(ERROR) << "Unable to get application data directory";
        return QString();
    }

    return dir_path;
}

//--------------------------------------------------------------------------------------------------
QByteArray BookDatabase::configValue(const QString& key) const
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return QByteArray();
    }

    QSqlQuery query(databaseByName(connection_name_));
    query.prepare("SELECT value FROM config WHERE key=?");
    query.addBindValue(key);

    if (!query.exec() || !query.next())
        return QByteArray();

    return query.value(0).toByteArray();
}

//--------------------------------------------------------------------------------------------------
bool BookDatabase::setConfigValue(const QString& key, const QByteArray& value)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    QSqlQuery query(databaseByName(connection_name_));
    query.prepare("INSERT OR REPLACE INTO config (key, value) VALUES (?, ?)");
    query.addBindValue(key);
    query.addBindValue(value);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << query.lastError();
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
QByteArray BookDatabase::encryptData(const QByteArray& data) const
{
    return encryptBlob(encryption_key_, data);
}

//--------------------------------------------------------------------------------------------------
QByteArray BookDatabase::decryptData(const QByteArray& encrypted) const
{
    return decryptBlob(encryption_key_, encrypted);
}

//--------------------------------------------------------------------------------------------------
bool BookDatabase::reencryptAll(const QByteArray& old_key, const QByteArray& new_key)
{
    QSqlDatabase db = databaseByName(connection_name_);
    if (!db.isValid())
        return false;

    if (!db.transaction())
    {
        LOG(ERROR) << "Unable to execute transaction:" << db.lastError();
        return false;
    }

    bool success = false;

    do
    {
        // Re-encrypt computers.
        {
            QSqlQuery select_query(db);
            if (!select_query.exec(
                "SELECT id, comment, address, username, password FROM computers"))
            {
                LOG(ERROR) << "Unable to read computers:" << select_query.lastError();
                break;
            }

            QSqlQuery update_query(db);
            update_query.prepare(
                "UPDATE computers SET comment=?, address=?, username=?, password=? WHERE id=?");

            while (select_query.next())
            {
                qint64 id = select_query.value(0).toLongLong();
                QByteArray comment = decryptBlob(old_key, select_query.value(1).toByteArray());
                QByteArray address = decryptBlob(old_key, select_query.value(2).toByteArray());
                QByteArray username = decryptBlob(old_key, select_query.value(3).toByteArray());
                QByteArray pw = decryptBlob(old_key, select_query.value(4).toByteArray());

                update_query.addBindValue(encryptBlob(new_key, comment));
                update_query.addBindValue(encryptBlob(new_key, address));
                update_query.addBindValue(encryptBlob(new_key, username));
                update_query.addBindValue(encryptBlob(new_key, pw));
                update_query.addBindValue(id);

                if (!update_query.exec())
                {
                    LOG(ERROR) << "Unable to re-encrypt computer:" << update_query.lastError();
                    goto rollback;
                }
            }
        }

        // Re-encrypt groups.
        {
            QSqlQuery select_query(db);
            if (!select_query.exec("SELECT id, comment FROM groups"))
            {
                LOG(ERROR) << "Unable to read groups:" << select_query.lastError();
                break;
            }

            QSqlQuery update_query(db);
            update_query.prepare("UPDATE groups SET comment=? WHERE id=?");

            while (select_query.next())
            {
                qint64 id = select_query.value(0).toLongLong();
                QByteArray comment = decryptBlob(old_key, select_query.value(1).toByteArray());

                update_query.addBindValue(encryptBlob(new_key, comment));
                update_query.addBindValue(id);

                if (!update_query.exec())
                {
                    LOG(ERROR) << "Unable to re-encrypt group:" << update_query.lastError();
                    goto rollback;
                }
            }
        }

        // Re-encrypt router config.
        {
            auto reencryptConfig = [&](const char* key_name) -> bool
            {
                QSqlQuery query(db);
                query.prepare("SELECT value FROM config WHERE key=?");
                query.addBindValue(key_name);

                if (!query.exec() || !query.next())
                    return true; // Key doesn't exist, skip.

                QByteArray value = decryptBlob(old_key, query.value(0).toByteArray());

                query.prepare("INSERT OR REPLACE INTO config (key, value) VALUES (?, ?)");
                query.addBindValue(key_name);
                query.addBindValue(encryptBlob(new_key, value));

                if (!query.exec())
                {
                    LOG(ERROR) << "Unable to re-encrypt config key:" << query.lastError();
                    return false;
                }
                return true;
            };

            if (!reencryptConfig(kKeyRouterAddress))
                break;
            if (!reencryptConfig(kKeyRouterUsername))
                break;
            if (!reencryptConfig(kKeyRouterPassword))
                break;
        }

        success = true;
    }
    while (false);

rollback:
    if (!success)
    {
        db.rollback();
        return false;
    }

    if (!db.commit())
    {
        LOG(ERROR) << "Unable to commit transaction:" << db.lastError();
        db.rollback();
        return false;
    }

    return true;
}

} // namespace client
