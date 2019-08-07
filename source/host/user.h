//
// Aspia Project
// Copyright (C) 2019 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef HOST__USER_H
#define HOST__USER_H

#include <QByteArray>
#include <QList>

namespace host {

class User
{
public:
    enum Flags { ENABLED = 1 };

    static User create(const QString& name, const QString& password);

    bool isValid() const;

    QString name;
    QByteArray salt;
    QByteArray verifier;
    QByteArray number;
    QByteArray generator;
    uint32_t sessions = 0;
    uint32_t flags = 0;
};

class UserList
{
public:
    UserList() = default;

    void add(const User& user);
    void remove(const QString& username);
    void remove(int index);
    void update(int index, const User& user);

    int find(const QString& username) const;
    int count() const { return list_.count(); }
    const User& at(int index) const;

    const QByteArray& seedKey() const { return seed_key_; }
    void setSeedKey(const QByteArray& seed_key);

    bool hasIndex(int index) const;

private:
    QByteArray seed_key_;
    QList<User> list_;
};

} // namespace host

#endif // HOST__USER_H
