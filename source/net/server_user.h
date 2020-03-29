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

#ifndef NET__SERVER_USER_H
#define NET__SERVER_USER_H

#include "base/memory/byte_array.h"

namespace net {

class ServerUser
{
public:
    ServerUser() = default;
    ~ServerUser() = default;

    ServerUser(const ServerUser& other) = default;
    ServerUser& operator=(const ServerUser& other) = default;

    ServerUser(ServerUser&& other) noexcept = default;
    ServerUser& operator=(ServerUser&& other) noexcept = default;

    enum Flags { ENABLED = 1 };

    static ServerUser create(std::u16string_view name, std::u16string_view password);
    bool isValid() const;

    std::u16string name;
    base::ByteArray salt;
    base::ByteArray verifier;
    base::ByteArray number;
    base::ByteArray generator;
    uint32_t sessions = 0;
    uint32_t flags = 0;
};

class ServerUserList
{
public:
    ServerUserList() = default;

    ServerUserList(const ServerUserList& other) = default;
    ServerUserList& operator=(const ServerUserList& other) = default;

    ServerUserList(ServerUserList&& other) noexcept = default;
    ServerUserList& operator=(ServerUserList&& other) noexcept = default;

    void add(const ServerUser& user);
    void add(ServerUser&& user);
    void merge(const ServerUserList& user_list);
    void merge(ServerUserList&& user_list);

    const ServerUser& find(std::u16string_view username) const;
    size_t count() const { return list_.size(); }
    bool empty() const { return list_.empty(); }

    const base::ByteArray& seedKey() const { return seed_key_; }
    void setSeedKey(const base::ByteArray& seed_key);
    void setSeedKey(base::ByteArray&& seed_key);

    class Iterator
    {
    public:
        Iterator(const ServerUserList& list);
        ~Iterator();

        const ServerUser& user() const;
        bool isAtEnd() const;
        void advance();

    private:
        const std::vector<ServerUser>& list_;
        std::vector<ServerUser>::const_iterator pos_;
    };

private:
    base::ByteArray seed_key_;
    std::vector<ServerUser> list_;
};

} // namespace net

#endif // NET__SERVER_USER_H
