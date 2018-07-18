//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef _ASPIA_HOST__USER_H
#define _ASPIA_HOST__USER_H

#include <QString>

namespace aspia {

class User
{
public:
    ~User();

    enum Flags { FLAG_ENABLED = 1 };

    static const int kMaxUserNameLength = 64;
    static const int kMinPasswordLength = 8;
    static const int kMaxPasswordLength = 64;
    static const int kPasswordHashLength = 64;

    static bool isValidName(const QString& value);
    static bool isValidPassword(const QString& value);

    bool setName(const QString& value);
    const QString& name() const { return name_; }

    bool setPassword(const QString& value);
    bool setPasswordHash(const QByteArray& value);
    const QByteArray& passwordHash() const { return password_hash_; }

    void setFlags(quint32 value);
    quint32 flags() const { return flags_; }

    void setSessions(quint32 value);
    quint32 sessions() const { return sessions_; }

private:
    QString name_;
    QByteArray password_hash_;
    quint32 flags_ = 0;
    quint32 sessions_ = 0;
};

} // namespace aspia

#endif // _ASPIA_HOST__USER_H
