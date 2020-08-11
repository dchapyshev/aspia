//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef ROUTER__DATABASE_H
#define ROUTER__DATABASE_H

#include "peer/peer_id.h"
#include "peer/user.h"

namespace router {

class Database
{
public:
    virtual ~Database() = default;

    virtual peer::UserList userList() const = 0;
    virtual bool addUser(const peer::User& user) = 0;
    virtual bool modifyUser(const peer::User& user) = 0;
    virtual bool removeUser(int64_t entry_id) = 0;
    virtual peer::PeerId peerId(const base::ByteArray& keyHash) const = 0;
    virtual bool addPeer(const base::ByteArray& keyHash) = 0;
};

} // namespace router

#endif // ROUTER__DATABASE_H
