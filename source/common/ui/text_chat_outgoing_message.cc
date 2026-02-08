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

#include "common/ui/text_chat_outgoing_message.h"

#include <QLocale>
#include <QTime>

namespace common {

//--------------------------------------------------------------------------------------------------
TextChatOutgoingMessage::TextChatOutgoingMessage(QWidget* parent)
    : TextChatMessage(TextChatMessage::Direction::OUTGOING, parent)
{
    ui.setupUi(this);

    QString time = QLocale::system().toString(QTime::currentTime(), QLocale::ShortFormat);
    ui.label_time->setText(time);
}

//--------------------------------------------------------------------------------------------------
TextChatOutgoingMessage::~TextChatOutgoingMessage() = default;

//--------------------------------------------------------------------------------------------------
void TextChatOutgoingMessage::setMessageText(const QString& text)
{
    ui.label_message->setText(text);
    adjustSize();
}

//--------------------------------------------------------------------------------------------------
QString TextChatOutgoingMessage::messageText() const
{
    return ui.label_message->text();
}

//--------------------------------------------------------------------------------------------------
QString TextChatOutgoingMessage::messageTime() const
{
    return ui.label_time->text();
}

//--------------------------------------------------------------------------------------------------
void TextChatOutgoingMessage::resizeEvent(QResizeEvent* /* event */)
{
    adjustSize();
}

} // namespace common
