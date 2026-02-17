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

#include "common/ui/text_chat_status_message.h"

#include <QLocale>
#include <QTime>

namespace common {

//--------------------------------------------------------------------------------------------------
TextChatStatusMessage::TextChatStatusMessage(QWidget* parent)
    : TextChatMessage(TextChatMessage::Direction::STATUS, parent)
{
    ui.setupUi(this);
}

//--------------------------------------------------------------------------------------------------
TextChatStatusMessage::~TextChatStatusMessage() = default;

//--------------------------------------------------------------------------------------------------
void TextChatStatusMessage::setMessageText(const QString& text)
{
    ui.label_message->setText(text);
}

//--------------------------------------------------------------------------------------------------
QString TextChatStatusMessage::messageText() const
{
    return ui.label_message->text();
}

//--------------------------------------------------------------------------------------------------
QString TextChatStatusMessage::messageTime() const
{
    return QString();
}

} // namespace common
