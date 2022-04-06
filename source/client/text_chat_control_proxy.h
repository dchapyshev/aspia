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

#ifndef CLIENT_TEXT_CHAT_CONTROL_PROXY_H
#define CLIENT_TEXT_CHAT_CONTROL_PROXY_H

#include "base/macros_magic.h"
#include "client/text_chat_control.h"

#include <memory>

namespace base {
class TaskRunner;
} // namespace base

namespace client {

class TextChatControlProxy : public std::enable_shared_from_this<TextChatControlProxy>
{
public:
    TextChatControlProxy(std::shared_ptr<base::TaskRunner> io_task_runner,
                         TextChatControl* text_chat_control);
    ~TextChatControlProxy();

    void dettach();

    void onTextChatMessage(const proto::TextChat& text_chat);

private:
    std::shared_ptr<base::TaskRunner> io_task_runner_;
    TextChatControl* text_chat_control_;

    DISALLOW_COPY_AND_ASSIGN(TextChatControlProxy);
};

} // namespace client

#endif // CLIENT_TEXT_CHAT_CONTROL_PROXY_H
