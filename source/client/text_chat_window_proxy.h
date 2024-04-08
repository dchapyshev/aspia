//
// Aspia Project
// Copyright (C) 2016-2024 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef CLIENT_TEXT_CHAT_WINDOW_PROXY_H
#define CLIENT_TEXT_CHAT_WINDOW_PROXY_H

#include "base/macros_magic.h"
#include "client/text_chat_window.h"

#include <memory>

namespace base {
class TaskRunner;
} // namespace base

namespace client {

class TextChatWindowProxy final : public std::enable_shared_from_this<TextChatWindowProxy>
{
public:
    TextChatWindowProxy(std::shared_ptr<base::TaskRunner> ui_task_runner,
                        TextChatWindow* text_chat_window);
    ~TextChatWindowProxy();

    void dettach();

    void start(std::shared_ptr<TextChatControlProxy> text_chat_control_proxy);
    void onTextChatMessage(const proto::TextChat& text_chat);

private:
    std::shared_ptr<base::TaskRunner> ui_task_runner_;
    TextChatWindow* text_chat_window_;

    DISALLOW_COPY_AND_ASSIGN(TextChatWindowProxy);
};

} // namespace client

#endif // CLIENT_TEXT_CHAT_WINDOW_PROXY_H
