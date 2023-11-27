//
// Aspia Project
// Copyright (C) 2016-2023 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef HOST_CLIENT_SESSION_FILE_TRANSFER_H
#define HOST_CLIENT_SESSION_FILE_TRANSFER_H

#include "base/macros_magic.h"
#include "base/location.h"
#include "base/waitable_timer.h"
#include "base/ipc/ipc_channel.h"
#include "base/ipc/ipc_server.h"
#include "host/client_session.h"

#include <vector>

namespace host {

class ClientSessionFileTransfer
    : public ClientSession,
      public base::IpcServer::Delegate,
      public base::IpcChannel::Listener
{
public:
    ClientSessionFileTransfer(std::unique_ptr<base::TcpChannel> channel,
                              std::shared_ptr<base::TaskRunner> task_runner);
    ~ClientSessionFileTransfer() override;

protected:
    // ClientSession implementation.
    void onStarted() override;
    void onReceived(uint8_t channel_id, const base::ByteArray& buffer) override;
    void onWritten(uint8_t channel_id, size_t pending) override;

    // base::IpcServer::Delegate implementation.
    void onNewConnection(std::unique_ptr<base::IpcChannel> channel) override;
    void onErrorOccurred() override;

    // base::IpcChannel::Listener implemenation.
    void onIpcDisconnected() override;
    void onIpcMessageReceived(const base::ByteArray& buffer) override;

private:
    void onError(const base::Location& location);

    std::shared_ptr<base::TaskRunner> task_runner_;
    std::unique_ptr<base::WaitableTimer> attach_timer_;
    std::unique_ptr<base::IpcServer> ipc_server_;
    std::unique_ptr<base::IpcChannel> ipc_channel_;
    std::vector<base::ByteArray> pending_messages_;
    bool has_logged_on_user_ = false;

    DISALLOW_COPY_AND_ASSIGN(ClientSessionFileTransfer);
};

} // namespace host

#endif // HOST_CLIENT_SESSION_FILE_TRANSFER_H
