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
#include "base/crypto/secure_memory.h"
#include "base/logging.h"

#include <QDir>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QVariant>

#include <utility>

namespace client {

namespace {

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
bool createTables(QSqlDatabase& db)
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
        LOG(ERROR) << "Unable to create groups table:" << query.lastError();
        return false;
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
        LOG(ERROR) << "Unable to create computers table:" << query.lastError();
        return false;
    }

    if (!query.exec("PRAGMA foreign_keys = ON"))
    {
        LOG(ERROR) << "Unable to enable foreign keys:" << query.lastError();
        return false;
    }

    return true;
}

} // namespace

//--------------------------------------------------------------------------------------------------
const char BookDatabase::kConnectionName[] = "book";

//--------------------------------------------------------------------------------------------------
BookDatabase::BookDatabase() = default;

//--------------------------------------------------------------------------------------------------
BookDatabase::BookDatabase(const QByteArray& encryption_key)
    : encryption_key_(encryption_key),
      valid_(true)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
BookDatabase::BookDatabase(BookDatabase&& other) noexcept
    : encryption_key_(std::move(other.encryption_key_)),
      valid_(other.valid_)
{
    other.valid_ = false;
    other.encryption_key_.clear();
}

//--------------------------------------------------------------------------------------------------
BookDatabase& BookDatabase::operator=(BookDatabase&& other) noexcept
{
    if (this != &other)
    {
        base::memZero(&encryption_key_);

        encryption_key_ = std::move(other.encryption_key_);
        valid_ = other.valid_;
    }

    other.valid_ = false;
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
BookDatabase BookDatabase::open(const QByteArray& encryption_key)
{
    QString dir_path = databaseDirectory();
    if (dir_path.isEmpty())
    {
        LOG(ERROR) << "Invalid directory path";
        return BookDatabase();
    }

    // Ensure directory exists.
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

    bool need_create = !QFileInfo::exists(file_path);

    LOG(INFO) << (need_create ? "Creating" : "Opening") << "book database:" << file_path;

    QSqlDatabase db = QSqlDatabase::database(kConnectionName, false);
    if (!db.isValid())
    {
        db = QSqlDatabase::addDatabase("QSQLITE", kConnectionName);
        db.setDatabaseName(file_path);
    }

    if (!db.isOpen() && !db.open())
    {
        LOG(ERROR) << "QSqlDatabase::open failed:" << db.lastError();
        return BookDatabase();
    }

    if (need_create)
    {
        if (!createTables(db))
        {
            db.close();
            QSqlDatabase::removeDatabase(kConnectionName);
            QFile::remove(file_path);
            return BookDatabase();
        }
    }
    else
    {
        // Enable foreign keys for existing database.
        QSqlQuery query(db);
        if (!query.exec("PRAGMA foreign_keys = ON"))
            LOG(WARNING) << "Unable to enable foreign keys:" << query.lastError();
    }

    return BookDatabase(encryption_key);
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
bool BookDatabase::isValid() const
{
    return valid_;
}

//--------------------------------------------------------------------------------------------------
QList<ComputerData> BookDatabase::computerList(qint64 group_id) const
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return {};
    }

    QSqlQuery query(QSqlDatabase::database(kConnectionName, false));
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

    if (computer.address.isEmpty() || computer.name.isEmpty() || computer.id < 0 || computer.group_id < 0)
    {
        LOG(ERROR) << "Invalid parameters";
        return false;
    }

    QSqlQuery query(QSqlDatabase::database(kConnectionName, false));
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

    QSqlQuery query(QSqlDatabase::database(kConnectionName, false));
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

    QSqlQuery query(QSqlDatabase::database(kConnectionName, false));
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

    QSqlQuery query(QSqlDatabase::database(kConnectionName, false));
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
    QSqlQuery query(QSqlDatabase::database(kConnectionName, false));

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

    QSqlQuery query(QSqlDatabase::database(kConnectionName, false));
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

    QSqlQuery query(QSqlDatabase::database(kConnectionName, false));
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

    QSqlQuery query(QSqlDatabase::database(kConnectionName, false));
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

    QSqlQuery query(QSqlDatabase::database(kConnectionName, false));
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

    QSqlQuery query(QSqlDatabase::database(kConnectionName, false));
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

    QSqlQuery query(QSqlDatabase::database(kConnectionName, false));
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
bool BookDatabase::setEncryption(const QByteArray& encryption_key)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    QByteArray old_key = encryption_key_;

    if (!reencryptAll(old_key, encryption_key))
    {
        LOG(ERROR) << "Failed to re-encrypt data";
        return false;
    }

    encryption_key_ = encryption_key;

    base::memZero(&old_key);
    return true;
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
    QSqlDatabase db = QSqlDatabase::database(kConnectionName, false);
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
