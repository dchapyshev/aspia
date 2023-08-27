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

#include "client/text_chat_control_proxy.h"

#include "base/logging.h"
#include "base/task_runner.h"
#include "proto/text_chat.pb.h"

namespace client {

//--------------------------------------------------------------------------------------------------
TextChatControlProxy::TextChatControlProxy(std::shared_ptr<base::TaskRunner> io_task_runner,
                                           TextChatControl* text_chat_control)
    : io_task_runner_(std::move(io_task_runner)),
      text_chat_control_(text_chat_control)
{
    LOG(LS_INFO) << "Ctor";
    DCHECK(io_task_runner_);
    DCHECK(text_chat_control_);
}

//--------------------------------------------------------------------------------------------------
TextChatControlProxy::~TextChatControlProxy()
{
    LOG(LS_INFO) << "Dtor";
    DCHECK(!text_chat_control_);
}

//--------------------------------------------------------------------------------------------------
void TextChatControlProxy::dettach()
{
    LOG(LS_INFO) << "Dettach text chat control";
    DCHECK(io_task_runner_->belongsToCurrentThread());
    text_chat_control_ = nullptr;
}

//--------------------------------------------------------------------------------------------------
void TextChatControlProxy::onTextChatMessage(const proto::TextChat &text_chat)
{
    if (!io_task_runner_->belongsToCurrentThread())
    {
        io_task_runner_->postTask(
            std::bind(&TextChatControlProxy::onTextChatMessage, shared_from_this(), text_chat));
        return;
    }

    if (text_chat_control_)
        text_chat_control_->onTextChatMessage(text_chat);
}

} // namespace client
