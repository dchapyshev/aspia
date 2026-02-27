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

#ifndef COMMON_UI_CHAT_OUTGOING_MESSAGE_H
#define COMMON_UI_CHAT_OUTGOING_MESSAGE_H

#include "common/ui/chat_message.h"
#include "ui_chat_outgoing_message.h"

namespace common {

class ChatOutgoingMessage final : public ChatMessage
{
    Q_OBJECT

public:
    explicit ChatOutgoingMessage(QWidget* parent = nullptr);
    ~ChatOutgoingMessage() final;

    void setMessageText(const QString& text) final;
    QString messageText() const final;
    QString messageTime() const final;

protected:
    void resizeEvent(QResizeEvent* event) final;

private:
    Ui::ChatOutgoingMessage ui;
};

} // namespace common

#endif // COMMON_UI_CHAT_OUTGOING_MESSAGE_H
