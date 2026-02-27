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

#include "common/ui/chat_status_message.h"

#include <QLocale>
#include <QTime>

namespace common {

//--------------------------------------------------------------------------------------------------
ChatStatusMessage::ChatStatusMessage(QWidget* parent)
    : ChatMessage(ChatMessage::Direction::STATUS, parent)
{
    ui.setupUi(this);
}

//--------------------------------------------------------------------------------------------------
ChatStatusMessage::~ChatStatusMessage() = default;

//--------------------------------------------------------------------------------------------------
void ChatStatusMessage::setMessageText(const QString& text)
{
    ui.label_message->setText(text);
}

//--------------------------------------------------------------------------------------------------
QString ChatStatusMessage::messageText() const
{
    return ui.label_message->text();
}

//--------------------------------------------------------------------------------------------------
QString ChatStatusMessage::messageTime() const
{
    return QString();
}

} // namespace common
