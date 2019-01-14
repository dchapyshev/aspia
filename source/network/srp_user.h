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

#ifndef ASPIA_NETWORK__SRP_USER_H
#define ASPIA_NETWORK__SRP_USER_H

#include <QList>
#include <QByteArray>

namespace net {

struct SrpUser
{
    enum Flags { ENABLED = 1 };

    QString name;
    QByteArray salt;
    QByteArray verifier;
    QByteArray number;
    QByteArray generator;
    uint32_t sessions = 0;
    uint32_t flags = 0;
};

struct SrpUserList
{
    QByteArray seed_key;
    QList<SrpUser> list;
};

} // namespace net

#endif // ASPIA_NETWORK__SRP_USER_H
