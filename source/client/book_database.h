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

#ifndef CLIENT_BOOK_DATABASE_H
#define CLIENT_BOOK_DATABASE_H

#include <QByteArray>
#include <QList>
#include <QString>

#include <optional>

#include "client/book_data.h"
#include "client/router_config.h"

namespace client {

class BookDatabase
{
public:
    BookDatabase();
    BookDatabase(BookDatabase&& other) noexcept;
    BookDatabase& operator=(BookDatabase&& other) noexcept;
    ~BookDatabase();

    enum class EncryptionType { NONE, CHACHA20_POLY1305 };

    static BookDatabase create(EncryptionType encryption_type,
                               const QString& password = QString());
    static BookDatabase open(const QString& password = QString());
    static QString filePath();
    static bool isEncrypted();
    bool isValid() const;

    // Computers.
    QList<ComputerData> computerList(qint64 group_id) const;
    bool addComputer(ComputerData& computer);
    bool modifyComputer(const ComputerData& computer);
    bool removeComputer(qint64 computer_id);
    std::optional<ComputerData> findComputer(qint64 computer_id) const;

    // Search.
    QList<ComputerData> searchComputers(const QString& query) const;

    // Groups.
    QList<ComputerGroupData> groupList(qint64 parent_id) const;
    QList<ComputerGroupData> allGroups() const;
    bool addGroup(ComputerGroupData& group);
    bool modifyGroup(const ComputerGroupData& group);
    bool removeGroup(qint64 group_id);
    std::optional<ComputerGroupData> findGroup(qint64 group_id) const;

    // Configuration.
    EncryptionType encryptionType() const;
    bool setEncryption(EncryptionType encryption_type, const QString& password = QString());
    bool isRouterEnabled() const;
    void setRouterEnabled(bool enabled);
    RouterConfig routerConfig() const;
    void setRouterConfig(const RouterConfig& config);
    QString displayName() const;
    void setDisplayName(const QString& name);

private:
    explicit BookDatabase(const QString& connection_name, const QByteArray& encryption_key);

    static QString databaseDirectory();

    QByteArray configValue(const QString& key) const;
    bool setConfigValue(const QString& key, const QByteArray& value);

    QByteArray encryptData(const QByteArray& data) const;
    QByteArray decryptData(const QByteArray& encrypted) const;

    bool reencryptAll(const QByteArray& old_key, const QByteArray& new_key);

    QByteArray encryption_key_;
    QString connection_name_;

    Q_DISABLE_COPY(BookDatabase)
};

} // namespace client

#endif // CLIENT_BOOK_DATABASE_H
