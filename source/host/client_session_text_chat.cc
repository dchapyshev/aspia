//
// Aspia Project
// Copyright (C) 2016-2022 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "host/client_session_text_chat.h"

#include "base/logging.h"
#include "proto/text_chat.pb.h"

namespace host {

ClientSessionTextChat::ClientSessionTextChat(std::unique_ptr<base::NetworkChannel> channel)
    : ClientSession(proto::SESSION_TYPE_TEXT_CHAT, std::move(channel))
{
    LOG(LS_INFO) << "Ctor";
}

ClientSessionTextChat::~ClientSessionTextChat()
{
    LOG(LS_INFO) << "Dtor";
}

void ClientSessionTextChat::sendTextChat(const proto::TextChat& text_chat)
{
    sendMessage(base::serialize(text_chat));
}

void ClientSessionTextChat::onMessageReceived(const base::ByteArray& buffer)
{
    proto::TextChat text_chat;

    if (!base::parse(buffer, &text_chat))
    {
        LOG(LS_WARNING) << "Unable to parse system info request";
        return;
    }

    delegate_->onClientSessionTextChat(id(), text_chat);
}

void ClientSessionTextChat::onMessageWritten(size_t /* pending */)
{
    // Nothing
}

void ClientSessionTextChat::onStarted()
{
    // Nothing
}

} // namespace host
