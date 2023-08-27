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

#include "client/text_chat_window_proxy.h"

#include "base/logging.h"
#include "base/task_runner.h"
#include "client/text_chat_control_proxy.h"
#include "proto/text_chat.pb.h"

namespace client {

//--------------------------------------------------------------------------------------------------
TextChatWindowProxy::TextChatWindowProxy(std::shared_ptr<base::TaskRunner> ui_task_runner,
                                         TextChatWindow* text_chat_window)
    : ui_task_runner_(std::move(ui_task_runner)),
      text_chat_window_(text_chat_window)
{
    LOG(LS_INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
TextChatWindowProxy::~TextChatWindowProxy()
{
    LOG(LS_INFO) << "Dtor";
    DCHECK(!text_chat_window_);
}

//--------------------------------------------------------------------------------------------------
void TextChatWindowProxy::dettach()
{
    LOG(LS_INFO) << "Dettach text chat window";
    DCHECK(ui_task_runner_->belongsToCurrentThread());
    text_chat_window_ = nullptr;
}

//--------------------------------------------------------------------------------------------------
void TextChatWindowProxy::start(std::shared_ptr<TextChatControlProxy> text_chat_control_proxy)
{
    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(
            std::bind(&TextChatWindowProxy::start, shared_from_this(), text_chat_control_proxy));
        return;
    }

    if (text_chat_window_)
        text_chat_window_->start(text_chat_control_proxy);
}

//--------------------------------------------------------------------------------------------------
void TextChatWindowProxy::onTextChatMessage(const proto::TextChat& text_chat)
{
    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(
            std::bind(&TextChatWindowProxy::onTextChatMessage, shared_from_this(), text_chat));
        return;
    }

    if (text_chat_window_)
        text_chat_window_->onTextChatMessage(text_chat);
}

} // namespace client
