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

#include "common/ui/chat_incoming_message.h"

#include <QLocale>
#include <QTime>

#include "ui_chat_incoming_message.h"

//--------------------------------------------------------------------------------------------------
ChatIncomingMessage::ChatIncomingMessage(QWidget* parent)
    : ChatMessage(ChatMessage::Direction::INCOMING, parent),
      ui(std::make_unique<Ui::ChatIncomingMessage>())
{
    ui->setupUi(this);

    QString time = QLocale::system().toString(QTime::currentTime(), QLocale::ShortFormat);
    ui->label_time->setText(time);

    applyStyles(palette());
}

//--------------------------------------------------------------------------------------------------
ChatIncomingMessage::~ChatIncomingMessage() = default;

//--------------------------------------------------------------------------------------------------
void ChatIncomingMessage::setSource(const QString& source)
{
    ui->label_source->setText(source);
}

//--------------------------------------------------------------------------------------------------
QString ChatIncomingMessage::source() const
{
    return ui->label_source->text();
}

//--------------------------------------------------------------------------------------------------
void ChatIncomingMessage::setMessageText(const QString& text)
{
    ui->label_message->setText(text);
}

//--------------------------------------------------------------------------------------------------
QString ChatIncomingMessage::messageText() const
{
    return ui->label_message->text();
}

//--------------------------------------------------------------------------------------------------
QString ChatIncomingMessage::messageTime() const
{
    return ui->label_time->text();
}

//--------------------------------------------------------------------------------------------------
void ChatIncomingMessage::applyStyles(const QPalette& palette)
{
    const QColor& bubble = palette.color(QPalette::AlternateBase);
    const QColor& text = palette.color(QPalette::Text);

    ui->widget_message->setStyleSheet(QString(
        "#widget_message { background-color: %1; border: 1px solid %1; border-radius: 3px; }")
        .arg(bubble.name(QColor::HexArgb)));

    ui->label_message->setStyleSheet(QString(
        "#label_message { color: %1; font-family: Verdana; }")
        .arg(text.name(QColor::HexArgb)));
}
