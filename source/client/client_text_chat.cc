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
    LOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
ClientChat::~ClientChat()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void ClientChat::onChatMessage(const proto::chat::Chat& chat)
{
    sendSessionMessage(base::serialize(chat));
}

//--------------------------------------------------------------------------------------------------
void ClientChat::onSessionStarted()
{
    LOG(INFO) << "Text chat session started";
}

//--------------------------------------------------------------------------------------------------
void ClientChat::onSessionMessageReceived(const QByteArray& buffer)
{
    proto::chat::Chat chat;
    if (!base::parse(buffer, &chat))
    {
        LOG(ERROR) << "Unable to parse text chat message";
        return;
    }

    emit sig_chatMessage(chat);
}

//--------------------------------------------------------------------------------------------------
void ClientChat::onServiceMessageReceived(const QByteArray& /* buffer */)
{
    // Not used yet.
}

} // namespace client
