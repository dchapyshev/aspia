//
// Aspia Project
// Copyright (C) 2016-2024 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef HOST_CLIENT_SESSION_SYSTEM_INFO_H
#define HOST_CLIENT_SESSION_SYSTEM_INFO_H

#include "base/macros_magic.h"
#include "host/client_session.h"

namespace host {

class ClientSessionSystemInfo : public ClientSession
{
public:
    explicit ClientSessionSystemInfo(std::unique_ptr<base::TcpChannel> channel);
    ~ClientSessionSystemInfo() override;

protected:
    // ClientSession implementation.
    void onStarted() override;
    void onReceived(uint8_t channel_id, const base::ByteArray& buffer) override;
    void onWritten(uint8_t channel_id, size_t pending) override;

private:
    DISALLOW_COPY_AND_ASSIGN(ClientSessionSystemInfo);
};

} // namespace host

#endif // HOST_CLIENT_SESSION_SYSTEM_INFO_H
