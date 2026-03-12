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

#ifndef HOST_CHAT_CLIENT_H
#define HOST_CHAT_CLIENT_H

#include <QObject>

#include "base/net/tcp_channel.h"
#include "proto/chat.h"

namespace host {

class ChatClient final : public QObject
{
    Q_OBJECT

public:
    explicit ChatClient(base::TcpChannel* tcp_channel, QObject* parent = nullptr);
    ~ChatClient() final;

public slots:
    void start();
    void onSendChat(const proto::chat::Chat& chat);
    void onSendStatus(proto::chat::Status::Code code);

signals:
    void sig_started();
    void sig_finished();
    void sig_messageReceived(const proto::chat::Chat& chat);

private slots:
    void onTcpErrorOccurred(base::TcpChannel::ErrorCode error_code);
    void onTcpMessageReceived(quint8 tcp_channel_id, const QByteArray& buffer);

private:
    base::TcpChannel* tcp_channel_ = nullptr;
    Q_DISABLE_COPY_MOVE(ChatClient)
};

} // namespace host

#endif // HOST_CHAT_CLIENT_H
