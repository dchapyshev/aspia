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

#ifndef ASPIA_HOST__WIN__HOST_H_
#define ASPIA_HOST__WIN__HOST_H_

#include <QPointer>

#include "network/network_channel.h"
#include "protocol/authorization.pb.h"

namespace aspia {

class HostProcess;
class HostSessionFake;
class IpcChannel;
class IpcServer;
class NetworkChannel;

class Host : public QObject
{
    Q_OBJECT

public:
    enum State
    {
        StoppedState,
        StartingState,
        StoppingState,
        DetachedState,
        AttachedState
    };
    Q_ENUM(State);

    Host(QObject* parent = nullptr);
    ~Host();

    NetworkChannel* networkChannel() const { return network_channel_; }
    void setNetworkChannel(NetworkChannel* network_channel);

    proto::auth::SessionType sessionType() const { return session_type_; }
    void setSessionType(proto::auth::SessionType session_type);

    QString userName() const { return user_name_; }
    void setUserName(const QString& user_name);

    QString uuid() const { return uuid_; }
    void setUuid(const QString& uuid);

    QString remoteAddress() const;

    bool start();

public slots:
    void stop();
    void sessionChanged(quint32 event, quint32 session_id);

signals:
    void finished(Host* host);

protected:
    // QObject implementation.
    void timerEvent(QTimerEvent* event) override;

private slots:
    void networkMessageWritten(int message_id);
    void networkMessageReceived(const QByteArray& buffer);
    void ipcMessageWritten(int message_id);
    void ipcMessageReceived(const QByteArray& buffer);
    void ipcServerStarted(const QString& channel_id);
    void ipcNewConnection(IpcChannel* channel);
    void attachSession(quint32 session_id);
    void dettachSession();

private:
    bool startFakeSession();

    static const quint32 kInvalidSessionId = 0xFFFFFFFF;

    proto::auth::SessionType session_type_ = proto::auth::SESSION_TYPE_UNKNOWN;
    QString user_name_;
    QString uuid_;

    quint32 session_id_ = kInvalidSessionId;
    int attach_timer_id_ = 0;
    State state_ = StoppedState;

    QPointer<NetworkChannel> network_channel_;
    QPointer<IpcChannel> ipc_channel_;
    QPointer<HostProcess> session_process_;
    QPointer<HostSessionFake> fake_session_;

    Q_DISABLE_COPY(Host)
};

} // namespace aspia

#endif // ASPIA_HOST__WIN__HOST_H_
