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

#ifndef ROUTER_DATABASE_H
#define ROUTER_DATABASE_H

#include <QString>

#include "base/peer/host_id.h"
#include "base/peer/user.h"

namespace router {

class Database
{
public:
    Database();
    Database(Database&& other) noexcept;
    Database& operator=(Database&& other) noexcept;
    ~Database();

    enum class ErrorCode
    {
        SUCCESS = 0,
        UNKNOWN = 1,
        NO_HOST_FOUND = 2
    };

    static Database create();
    static Database open();
    static QString filePath();

    bool isValid() const;
    QVector<base::User> userList() const;
    bool addUser(const base::User& user);
    bool modifyUser(const base::User& user);
    bool removeUser(qint64 entry_id);
    base::User findUser(const QString& username) const;
    ErrorCode hostId(const QByteArray& key_hash, base::HostId* host_id) const;
    bool addHost(const QByteArray& key_hash);

private:
    explicit Database(const QString& connection_name);
    static QString databaseDirectory();

    QString connection_name_;

    Q_DISABLE_COPY(Database)
};

} // namespace router

#endif // ROUTER_DATABASE_H
