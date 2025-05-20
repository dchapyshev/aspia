//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef CLIENT_CLIENT_TEXT_CHAT_H
#define CLIENT_CLIENT_TEXT_CHAT_H

#include "client/client.h"
#include "proto/text_chat.h"

namespace client {

class ClientTextChat final : public Client
{
    Q_OBJECT

public:
    explicit ClientTextChat(QObject* parent = nullptr);
    ~ClientTextChat() final;

public slots:
    void onTextChatMessage(const proto::TextChat& text_chat);

signals:
    void sig_textChatMessage(const proto::TextChat& text_chat);

protected:
    // Client implementation.
    void onSessionStarted() final;
    void onSessionMessageReceived(const QByteArray& buffer) final;
    void onSessionMessageWritten(size_t pending) final;

private:
    DISALLOW_COPY_AND_ASSIGN(ClientTextChat);
};

} // namespace client

#endif // CLIENT_CLIENT_TEXT_CHAT_H
