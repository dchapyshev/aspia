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
#include "client/text_chat_control.h"

namespace client {

class TextChatControlProxy;
class TextChatWindowProxy;

class ClientTextChat final
    : public Client,
      public TextChatControl
{
public:
    explicit ClientTextChat(std::shared_ptr<base::TaskRunner> io_task_runner);
    ~ClientTextChat() final;

    void setTextChatWindow(std::shared_ptr<TextChatWindowProxy> text_chat_window_proxy);

    // TextChatControl implementation.
    void onTextChatMessage(const proto::TextChat& text_chat) final;

protected:
    // Client implementation.
    void onSessionStarted() final;
    void onSessionMessageReceived(uint8_t channel_id, const base::ByteArray& buffer) final;
    void onSessionMessageWritten(uint8_t channel_id, size_t pending) final;

private:
    std::shared_ptr<TextChatControlProxy> text_chat_control_proxy_;
    std::shared_ptr<TextChatWindowProxy> text_chat_window_proxy_;

    DISALLOW_COPY_AND_ASSIGN(ClientTextChat);
};

} // namespace client

#endif // CLIENT_CLIENT_TEXT_CHAT_H
