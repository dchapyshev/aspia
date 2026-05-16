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

#ifndef HOST_DATABASE_H
#define HOST_DATABASE_H

#include "base/peer/user.h"

#include <QByteArray>
#include <QString>
#include <QVector>

class SecureString;

class Database
{
public:
    ~Database() = default;

    static Database& instance();
    static QString directoryPath();
    static QString filePath();

    // Restricts access to the database directory and all files inside it to SYSTEM and elevated
    // administrators only. Creates the directory if it does not exist yet. Must be called once at
    // service startup before any database access.
    static bool applyPermissions();

    bool isValid() const;

    // Users.
    QVector<User> userList() const;
    User findUser(const QString& username) const;
    User findUser(qint64 entry_id) const;
    bool addUser(const User& user);
    bool modifyUser(const User& user);
    bool removeUser(qint64 entry_id);

    // Settings.
    QByteArray seedKey() const;
    bool setSeedKey(const QByteArray& seed_key);

    QByteArray passwordHash() const;
    bool setPasswordHash(const QByteArray& hash);

    QByteArray passwordHashSalt() const;
    bool setPasswordHashSalt(const QByteArray& salt);

    static bool createPasswordHash(const SecureString& password, QByteArray* hash, QByteArray* salt);
    static bool isValidPassword(const SecureString& password);

private:
    Database() = default;

    bool openDatabase();

    QString readSetting(const QString& name) const;
    bool writeSetting(const QString& name, const QString& value);

    bool valid_ = false;

    Q_DISABLE_COPY(Database)
};

#endif // HOST_DATABASE_H
