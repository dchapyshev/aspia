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

#ifndef HOST_WIN_PORTABLE_DESKTOP_CLIENT_H
#define HOST_WIN_PORTABLE_DESKTOP_CLIENT_H

#include <optional>

#include "host/client.h"
#include "proto/desktop_control.h"
#include "proto/desktop_internal.h"

// Desktop client of the portable host running with the rights of the user.
class PortableDesktopClient final : public Client
{
    Q_OBJECT

public:
    explicit PortableDesktopClient(TcpChannel* tcp_channel, QObject* parent = nullptr);
    ~PortableDesktopClient() final;

    void readDesktopMessage(quint32 channel_id, const QByteArray& buffer, bool reliable);

public slots:
    void onUserMessage(quint8 channel_id, const QByteArray& buffer);

signals:
    void sig_desktopMessage(quint32 channel_id, const QByteArray& buffer);
    void sig_userMessage(quint8 channel_id, const QByteArray& buffer);

protected:
    // Client implementation.
    void onStart() final;
    void onMessage(quint8 channel_id, const QByteArray& buffer) final;
    void onBandwidthChanged(qint64 bandwidth) final;
    void onTimer(TimePoint now) final;

private:
    void sendDesktopSessionMessage(quint8 net_channel_id, const QByteArray& buffer);
    void sendDesktopServiceMessage(const QByteArray& buffer);
    void readFeedback(const proto::control::Feedback& feedback);

    bool overflow_detection_enabled_ = false;
    proto::desktop::Overflow::State last_state_ = proto::desktop::Overflow::STATE_NONE;
    std::optional<proto::control::Config> config_;

    bool force_reliable_ = false;

    Q_DISABLE_COPY_MOVE(PortableDesktopClient)
};

#endif // HOST_WIN_PORTABLE_DESKTOP_CLIENT_H
