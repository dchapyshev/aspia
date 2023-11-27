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

#include "client/client_text_chat.h"

#include "base/logging.h"
#include "client/text_chat_control_proxy.h"
#include "client/text_chat_window_proxy.h"
#include "proto/text_chat.pb.h"

namespace client {

//--------------------------------------------------------------------------------------------------
ClientTextChat::ClientTextChat(std::shared_ptr<base::TaskRunner> io_task_runner)
    : Client(io_task_runner),
      text_chat_control_proxy_(std::make_shared<TextChatControlProxy>(io_task_runner, this))
{
    LOG(LS_INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
ClientTextChat::~ClientTextChat()
{
    LOG(LS_INFO) << "Dtor";
    text_chat_control_proxy_->dettach();
}

//--------------------------------------------------------------------------------------------------
void ClientTextChat::setTextChatWindow(
    std::shared_ptr<TextChatWindowProxy> text_chat_window_proxy)
{
    LOG(LS_INFO) << "Text chat window installed";
    text_chat_window_proxy_ = std::move(text_chat_window_proxy);
}

//--------------------------------------------------------------------------------------------------
void ClientTextChat::onTextChatMessage(const proto::TextChat& text_chat)
{
    sendMessage(proto::HOST_CHANNEL_ID_SESSION, text_chat);
}

//--------------------------------------------------------------------------------------------------
void ClientTextChat::onSessionStarted(const base::Version& /* peer_version */)
{
    LOG(LS_INFO) << "Text chat session started";
    text_chat_window_proxy_->start(text_chat_control_proxy_);
}

//--------------------------------------------------------------------------------------------------
void ClientTextChat::onSessionMessageReceived(
    uint8_t /* channel_id */, const base::ByteArray& buffer)
{
    proto::TextChat text_chat;
    if (!base::parse(buffer, &text_chat))
    {
        LOG(LS_ERROR) << "Unable to parse text chat message";
        return;
    }

    text_chat_window_proxy_->onTextChatMessage(text_chat);
}

//--------------------------------------------------------------------------------------------------
void ClientTextChat::onSessionMessageWritten(uint8_t /* channel_id */, size_t /* pending */)
{
    // Nothing
}

} // namespace client
