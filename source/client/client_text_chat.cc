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

#include "client/client_text_chat.h"

#include "base/logging.h"
#include "base/serialization.h"

namespace client {

//--------------------------------------------------------------------------------------------------
ClientTextChat::ClientTextChat(QObject* parent)
    : Client(parent)
{
    LOG(LS_INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
ClientTextChat::~ClientTextChat()
{
    LOG(LS_INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void ClientTextChat::onTextChatMessage(const proto::TextChat& text_chat)
{
    sendMessage(proto::peer::HOST_CHANNEL_ID_SESSION, base::serialize(text_chat));
}

//--------------------------------------------------------------------------------------------------
void ClientTextChat::onSessionStarted()
{
    LOG(LS_INFO) << "Text chat session started";
}

//--------------------------------------------------------------------------------------------------
void ClientTextChat::onSessionMessageReceived(const QByteArray& buffer)
{
    proto::TextChat text_chat;
    if (!base::parse(buffer, &text_chat))
    {
        LOG(LS_ERROR) << "Unable to parse text chat message";
        return;
    }

    emit sig_textChatMessage(text_chat);
}

//--------------------------------------------------------------------------------------------------
void ClientTextChat::onSessionMessageWritten(size_t /* pending */)
{
    // Nothing
}

} // namespace client
