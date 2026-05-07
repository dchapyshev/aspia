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

#include "common/ui/chat_outgoing_message.h"

#include <QLocale>
#include <QTime>

//--------------------------------------------------------------------------------------------------
ChatOutgoingMessage::ChatOutgoingMessage(QWidget* parent)
    : ChatMessage(ChatMessage::Direction::OUTGOING, parent)
{
    ui.setupUi(this);

    QString time = QLocale::system().toString(QTime::currentTime(), QLocale::ShortFormat);
    ui.label_time->setText(time);

    applyStyles(palette());
}

//--------------------------------------------------------------------------------------------------
ChatOutgoingMessage::~ChatOutgoingMessage() = default;

//--------------------------------------------------------------------------------------------------
void ChatOutgoingMessage::setMessageText(const QString& text)
{
    ui.label_message->setText(text);
    adjustSize();
}

//--------------------------------------------------------------------------------------------------
QString ChatOutgoingMessage::messageText() const
{
    return ui.label_message->text();
}

//--------------------------------------------------------------------------------------------------
QString ChatOutgoingMessage::messageTime() const
{
    return ui.label_time->text();
}

//--------------------------------------------------------------------------------------------------
void ChatOutgoingMessage::resizeEvent(QResizeEvent* /* event */)
{
    adjustSize();
}

//--------------------------------------------------------------------------------------------------
void ChatOutgoingMessage::applyStyles(const QPalette& palette)
{
    const QColor& bubble = palette.color(QPalette::Midlight);
    const QColor& text = palette.color(QPalette::Text);

    ui.label_message->setStyleSheet(QString(
        "#label_message { color: %1; background-color: %2; border: 1px solid %2; "
        "border-radius: 4px; font-family: Verdana; padding: 6px; }")
        .arg(text.name(QColor::HexArgb), bubble.name(QColor::HexArgb)));
}
