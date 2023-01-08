//
// Aspia Project
// Copyright (C) 2016-2023 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef HOST_CLIENT_SESSION_TEXT_CHAT_H
#define HOST_CLIENT_SESSION_TEXT_CHAT_H

#include "base/macros_magic.h"
#include "host/client_session.h"

namespace host {

class ClientSessionTextChat : public ClientSession
{
public:
    explicit ClientSessionTextChat(std::unique_ptr<base::NetworkChannel> channel);
    ~ClientSessionTextChat() override;

    void sendTextChat(const proto::TextChat& text_chat);
    void sendStatus(proto::TextChatStatus::Status status);

    bool hasUser() const;
    void setHasUser(bool enable);

protected:
    // net::Listener implementation.
    void onMessageReceived(const base::ByteArray& buffer) override;
    void onMessageWritten(size_t pending) override;

    // ClientSession implementation.
    void onStarted() override;

private:
    bool has_user_ = false;

    DISALLOW_COPY_AND_ASSIGN(ClientSessionTextChat);
};

} // namespace host

#endif // HOST_CLIENT_SESSION_TEXT_CHAT_H
