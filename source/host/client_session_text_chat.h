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

#ifndef HOST_CLIENT_SESSION_TEXT_CHAT_H
#define HOST_CLIENT_SESSION_TEXT_CHAT_H

#include "host/client_session.h"

namespace host {

class ClientSessionTextChat final : public ClientSession
{
    Q_OBJECT

public:
    ClientSessionTextChat(base::TcpChannel* channel, QObject* parent);
    ~ClientSessionTextChat() final;

    void sendTextChat(const proto::text_chat::TextChat& text_chat);
    void sendStatus(proto::text_chat::Status::Code code);

    bool hasUser() const;
    void setHasUser(bool enable);

protected:
    // ClientSession implementation.
    void onStarted() final;
    void onReceived(const QByteArray& buffer) final;

private:
    bool has_user_ = false;

    Q_DISABLE_COPY(ClientSessionTextChat)
};

} // namespace host

#endif // HOST_CLIENT_SESSION_TEXT_CHAT_H
