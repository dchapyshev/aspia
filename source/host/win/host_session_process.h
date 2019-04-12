//
// Aspia Project
// Copyright (C) 2019 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef HOST__WIN__HOST_SESSION_PROCESS_H
#define HOST__WIN__HOST_SESSION_PROCESS_H

#include "base/win/session_id.h"
#include "base/win/session_status.h"
#include "host/win/host_process.h"
#include "proto/common.pb.h"

namespace ipc {
class Channel;
class Server;
} // namespace ipc

namespace net {
class ChannelHost;
} // namespace net

namespace host {

class SessionFake;

class SessionProcess : public QObject
{
    Q_OBJECT

public:
    SessionProcess(QObject* parent = nullptr);
    ~SessionProcess();

    enum class State { STOPPED, STARTING, STOPPING, DETACHED, ATTACHED };

    State state() const { return state_; }
    base::win::SessionId sessionId() const { return session_id_; }

    net::ChannelHost* networkChannel() const { return network_channel_; }
    void setNetworkChannel(net::ChannelHost* network_channel);

    const std::string& uuid() const { return uuid_; }
    void setUuid(const std::string& uuid);
    void setUuid(std::string&& uuid);

    const QString& userName() const;
    proto::SessionType sessionType() const;

    QString remoteAddress() const;

    bool start(base::win::SessionId session_id);

public slots:
    void stop();
    void setSessionEvent(base::win::SessionStatus status, base::win::SessionId session_id);
    void attachSession(base::win::SessionId session_id);
    void dettachSession();

signals:
    void finished();

protected:
    // QObject implementation.
    void timerEvent(QTimerEvent* event) override;

private slots:
    void ipcNewConnection(ipc::Channel* channel);

private:
    bool startFakeSession();

    std::string uuid_;

    base::win::SessionId session_id_ = base::win::kInvalidSessionId;
    int attach_timer_id_ = 0;
    State state_ = State::STOPPED;

    net::ChannelHost* network_channel_ = nullptr;
    QPointer<ipc::Server> ipc_server_;
    QPointer<ipc::Channel> ipc_channel_;
    QPointer<HostProcess> session_process_;
    QPointer<SessionFake> fake_session_;

    DISALLOW_COPY_AND_ASSIGN(SessionProcess);
};

} // namespace host

#endif // HOST__WIN__HOST_SESSION_PROCESS_H
