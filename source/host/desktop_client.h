//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef HOST_DESKTOP_CLIENT_H
#define HOST_DESKTOP_CLIENT_H

#include <QObject>
#include <QTime>

#include <optional>

#include "base/scoped_qpointer.h"
#include "base/session_id.h"
#include "host/client.h"
#include "proto/desktop_control.h"
#include "proto/desktop_internal.h"

namespace proto::power {
class Control;
} // namespace proto::power

namespace proto::task_manager {
class ClientToHost;
class HostToClient;
} // namespace proto::task_manager

class IpcChannel;
class IpcServer;
class QTimer;
class TaskManager;

class DesktopClient final : public Client
{
    Q_OBJECT

public:
    explicit DesktopClient(TcpChannel* tcp_channel, QObject* parent = nullptr);
    ~DesktopClient() final;

    bool isAttached() const;
    QString attach();
    void dettach();

public slots:
    void onUserMessage(quint8 channel_id, const QByteArray& buffer);

signals:
    void sig_switchSession(SessionId session_id);
    void sig_userMessage(quint8 channel_id, const QByteArray& buffer);

protected:
    void onStart() final;
    void onMessage(quint8 channel_id, const QByteArray& buffer) final;
    void onBandwidthChanged(qint64 bandwidth) final;
    void onTimer(const TimePoint& now) final;

private slots:
    void onIpcNewConnection();
    void onIpcErrorOccurred();
    void onIpcMessageReceived(quint32 channel_id, const QByteArray& buffer, bool reliable);
    void onIpcDisconnected();
    void onTaskManagerMessage(const proto::task_manager::HostToClient& message);

private:
    void sendIpcSessionMessage(quint8 net_channel_id, const QByteArray& buffer);
    void sendIpcServiceMessage(const QByteArray& buffer);
    void sendSessionList();
    void readFeedback(const proto::control::Feedback& feedback);
    void readPowerControl(const proto::power::Control& control);
    void readTaskManager(const proto::task_manager::ClientToHost& message);

    QTime dettach_time_;
    ScopedQPointer<IpcServer> ipc_server_;
    ScopedQPointer<IpcChannel> ipc_channel_;
    QTimer* fake_capture_timer_ = nullptr;

    bool overflow_detection_enabled_ = false;
    proto::desktop::Overflow::State last_state_ = proto::desktop::Overflow::STATE_NONE;
    std::optional<proto::control::Capabilities> capabilities_;
    std::optional<proto::control::Config> config_;

    bool force_reliable_ = false;

    TaskManager* task_manager_ = nullptr;

    Q_DISABLE_COPY_MOVE(DesktopClient)
};

#endif // HOST_DESKTOP_CLIENT_H
