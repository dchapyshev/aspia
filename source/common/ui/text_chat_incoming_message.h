//
// SmartCafe Project
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

#ifndef COMMON_UI_TEXT_CHAT_INCOMING_MESSAGE_H
#define COMMON_UI_TEXT_CHAT_INCOMING_MESSAGE_H

#include "common/ui/text_chat_message.h"
#include "ui_text_chat_incoming_message.h"

namespace common {

class TextChatIncomingMessage final : public TextChatMessage
{
    Q_OBJECT

public:
    explicit TextChatIncomingMessage(QWidget* parent = nullptr);
    ~TextChatIncomingMessage() final;

    void setSource(const QString& source);
    QString source() const;

    void setMessageText(const QString& text) final;
    QString messageText() const final;
    QString messageTime() const final;

private:
    Ui::TextChatIncomingMessage ui;
};

} // namespace common

#endif // COMMON_UI_TEXT_CHAT_INCOMING_MESSAGE_H
