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

#ifndef HOST__CLIENT_SESSION_FILE_TRANSFER_H
#define HOST__CLIENT_SESSION_FILE_TRANSFER_H

#include "base/macros_magic.h"
#include "host/client_session.h"

namespace host {

class ClientSessionFileTransfer : public ClientSession
{
public:
    explicit ClientSessionFileTransfer(std::unique_ptr<net::Channel> channel);
    ~ClientSessionFileTransfer();

protected:
    // net::Listener implementation.
    void onMessageReceived(const base::ByteArray& buffer) override;
    void onMessageWritten() override;

    // ClientSession implementation.
    void onStarted() override;

private:
    class Worker;
    std::unique_ptr<Worker> worker_;

    DISALLOW_COPY_AND_ASSIGN(ClientSessionFileTransfer);
};

} // namespace host

#endif // HOST__CLIENT_SESSION_FILE_TRANSFER_H
