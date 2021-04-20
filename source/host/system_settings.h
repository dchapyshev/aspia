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

#ifndef HOST__SYSTEM_SETTINGS_H
#define HOST__SYSTEM_SETTINGS_H

#include "base/macros_magic.h"
#include "base/settings/json_settings.h"

#include <filesystem>

namespace base {
class UserList;
} // namespace peer

namespace host {

class SystemSettings
{
public:
    SystemSettings();
    ~SystemSettings();

    const std::filesystem::path& filePath() const;
    bool isWritable() const;
    void sync();
    void flush();

    uint16_t tcpPort() const;
    void setTcpPort(uint16_t port);

    bool isRouterEnabled() const;
    void setRouterEnabled(bool enable);

    std::u16string routerAddress() const;
    void setRouterAddress(const std::u16string& address);

    uint16_t routerPort() const;
    void setRouterPort(uint16_t port);

    base::ByteArray routerPublicKey() const;
    void setRouterPublicKey(const base::ByteArray& key);

    std::unique_ptr<base::UserList> userList() const;
    void setUserList(const base::UserList& user_list);

    std::u16string updateServer() const;
    void setUpdateServer(const std::u16string& server);

    uint32_t preferredVideoCapturer() const;
    void setPreferredVideoCapturer(uint32_t type);

private:
    base::JsonSettings settings_;

    DISALLOW_COPY_AND_ASSIGN(SystemSettings);
};

} // namespace host

#endif // HOST__SYSTEM_SETTINGS_H
