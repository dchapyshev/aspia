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

#ifndef CLIENT_CLIENT_CHAT_H
#define CLIENT_CLIENT_CHAT_H

#include "client/client.h"
#include "proto/chat.h"

namespace client {

class ClientChat final : public Client
{
    Q_OBJECT

public:
    explicit ClientChat(QObject* parent = nullptr);
    ~ClientChat() final;

public slots:
    void onChatMessage(const proto::chat::Chat& chat);

signals:
    void sig_chatMessage(const proto::chat::Chat& chat);

protected:
    // Client implementation.
    void onSessionStarted() final;
    void onSessionMessageReceived(const QByteArray& buffer) final;
    void onServiceMessageReceived(const QByteArray& buffer) final;

private:
    Q_DISABLE_COPY_MOVE(ClientChat)
};

} // namespace client

#endif // CLIENT_CLIENT_CHAT_H
