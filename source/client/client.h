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

#ifndef CLIENT__CLIENT_H
#define CLIENT__CLIENT_H

#include "base/version.h"
#include "client/client_config.h"
#include "base/net/network_channel.h"

namespace base {
class TaskRunner;
} // namespace base

namespace peer {
class ClientAuthenticator;
} // namespace peer

namespace client {

class StatusWindow;
class StatusWindowProxy;

class Client : public base::NetworkChannel::Listener
{
public:
    explicit Client(std::shared_ptr<base::TaskRunner> io_task_runner);
    virtual ~Client();

    // Starts a session.
    void start(const Config& config);

    // Stops a session.
    void stop();

    // Sets the implementation of the status window.
    // The method must be called before calling method start().
    void setStatusWindow(std::shared_ptr<StatusWindowProxy> status_window_proxy);

    // Returns the version of the current client.
    static base::Version version();

    Config config() const { return config_; }

protected:
    std::u16string computerName() const;
    proto::SessionType sessionType() const;

    // Indicates that the session is started.
    // When calling this method, the client implementation should display a session window.
    virtual void onSessionStarted(const base::Version& peer_version) = 0;

    // Sends outgoing message.
    void sendMessage(const google::protobuf::MessageLite& message);

    // Methods for obtaining network metrics.
    int64_t totalRx() const;
    int64_t totalTx() const;
    int speedRx();
    int speedTx();

    // base::NetworkChannel::Listener implementation.
    void onConnected() override;
    void onDisconnected(base::NetworkChannel::ErrorCode error_code) override;

private:
    std::shared_ptr<base::TaskRunner> io_task_runner_;

    std::unique_ptr<base::NetworkChannel> channel_;
    std::unique_ptr<peer::ClientAuthenticator> authenticator_;
    std::shared_ptr<StatusWindowProxy> status_window_proxy_;

    Config config_;
};

} // namespace client

#endif // CLIENT__CLIENT_H
