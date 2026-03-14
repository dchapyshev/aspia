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

#include "host/client.h"
#include "proto/chat.h"

namespace host {

class ChatClient final : public Client
{
    Q_OBJECT

public:
    explicit ChatClient(base::TcpChannel* tcp_channel, QObject* parent = nullptr);
    ~ChatClient() final;

    void start() final;

public slots:
    void onSendChat(const proto::chat::Chat& chat);
    void onSendStatus(proto::chat::Status_Code code);

signals:
    void sig_messageReceived(const proto::chat::Chat& chat);

protected:
    void onMessage(quint8 channel_id, const QByteArray& buffer) final;

private:
    Q_DISABLE_COPY_MOVE(ChatClient)
};

} // namespace host

#endif // HOST_CHAT_CLIENT_H
