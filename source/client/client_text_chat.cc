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

#include "client/client_text_chat.h"

#include "base/logging.h"
#include "base/serialization.h"

namespace client {

//--------------------------------------------------------------------------------------------------
ClientChat::ClientChat(QObject* parent)
    : Client(parent)
{
    CLOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
ClientChat::~ClientChat()
{
    CLOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void ClientChat::onChatMessage(const proto::chat::Chat& chat)
{
    sendMessage(proto::peer::CHANNEL_ID_0, base::serialize(chat));
}

//--------------------------------------------------------------------------------------------------
void ClientChat::onStarted()
{
    CLOG(INFO) << "Text chat session started";
}

//--------------------------------------------------------------------------------------------------
void ClientChat::onMessageReceived(quint8 channel_id, const QByteArray& buffer)
{
    if (channel_id != proto::peer::CHANNEL_ID_0)
        return;

    proto::chat::Chat chat;
    if (!base::parse(buffer, &chat))
    {
        CLOG(ERROR) << "Unable to parse text chat message";
        return;
    }

    emit sig_chatMessage(chat);
}

} // namespace client
