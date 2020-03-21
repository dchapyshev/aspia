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
#include "base/threading/thread.h"
#include "client/client_config.h"
#include "net/network_listener.h"

namespace base {
class TaskRunner;
} // namespace base

namespace net {
class Channel;
} // namespace net

namespace client {

class Authenticator;
class StatusWindow;
class StatusWindowProxy;

class Client
    : public base::Thread::Delegate,
      public net::Listener
{
public:
    explicit Client(std::shared_ptr<base::TaskRunner> ui_task_runner);
    virtual ~Client();

    // Starts a session.
    bool start(const Config& config);

    // Stops a session.
    void stop();

    // Sets the implementation of the status window.
    // The method must be called before calling method start().
    void setStatusWindow(StatusWindow* status_window);

    // Returns the version of the current client.
    static base::Version version();

    Config config() const { return config_; }

protected:
    std::shared_ptr<base::TaskRunner> ioTaskRunner() const;
    std::shared_ptr<base::TaskRunner> uiTaskRunner() const;

    std::u16string computerName() const;
    proto::SessionType sessionType() const;

    // Indicates that the session is started.
    // When calling this method, the client implementation should display a session window.
    virtual void onSessionStarted(const base::Version& peer_version) = 0;

    // Indicates that the session is stopped.
    // When calling this method, the client implementation must destroy the session window.
    virtual void onSessionStopped() = 0;

    // Sends outgoing message.
    void sendMessage(const google::protobuf::MessageLite& message);

    // base::Thread::Delegate implementation.
    void onBeforeThreadRunning() override;
    void onAfterThreadRunning() override;

    // net::Listener implementation.
    void onConnected() override;
    void onDisconnected(net::ErrorCode error_code) override;

private:
    base::Thread io_thread_;
    std::shared_ptr<base::TaskRunner> io_task_runner_;
    std::shared_ptr<base::TaskRunner> ui_task_runner_;

    std::unique_ptr<net::Channel> channel_;
    std::unique_ptr<Authenticator> authenticator_;
    std::unique_ptr<StatusWindowProxy> status_window_proxy_;

    bool session_started_ = false;
    Config config_;
};

} // namespace client

#endif // CLIENT__CLIENT_H
