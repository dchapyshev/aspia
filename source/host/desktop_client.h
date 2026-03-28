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

#include "base/session_id.h"
#include "host/client.h"
#include "proto/desktop_internal.h"

class QTimer;

namespace base {
class IpcChannel;
class IpcServer;
} // namespace base

namespace host {

class DesktopClient final : public Client
{
    Q_OBJECT

public:
    explicit DesktopClient(base::TcpChannel* tcp_channel, QObject* parent = nullptr);
    ~DesktopClient() final;

    bool isAttached() const;
    QString attach();
    void dettach();

public slots:
    void onClipboardData(const QByteArray& buffer);

signals:
    void sig_switchSession(base::SessionId session_id);
    void sig_recordingChanged(bool started);
    void sig_clipboardData(const QByteArray& buffer);

protected:
    void onStart() final;
    void onMessage(quint8 channel_id, const QByteArray& buffer) final;
    void onBandwidthChanged(qint64 bandwidth) final;

private slots:
    void onIpcNewConnection();
    void onIpcErrorOccurred();
    void onIpcMessageReceived(quint32 channel_id, const QByteArray& buffer, bool reliable);
    void onIpcDisconnected();
    void onOverflowCheck();

private:
    void sendIpcServiceMessage(const QByteArray& buffer);
    void sendSessionList();

    QTime dettach_time_;
    base::IpcServer* ipc_server_ = nullptr;
    base::IpcChannel* ipc_channel_ = nullptr;
    QTimer* fake_capture_timer_ = nullptr;
    QTimer* overflow_timer_ = nullptr;

    proto::desktop::Overflow::State last_state_ = proto::desktop::Overflow::STATE_NONE;
    proto::control::Config config_;

    Q_DISABLE_COPY_MOVE(DesktopClient)
};

} // namespace host

#endif // HOST_DESKTOP_CLIENT_H
