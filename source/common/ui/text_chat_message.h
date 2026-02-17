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

#ifndef COMMON_UI_TEXT_CHAT_MESSAGE_H
#define COMMON_UI_TEXT_CHAT_MESSAGE_H

#include <QWidget>

namespace common {

class TextChatMessage : public QWidget
{
    Q_OBJECT

public:
    enum class Direction
    {
        INCOMING = 0,
        OUTGOING = 1,
        STATUS   = 2
    };

    explicit TextChatMessage(Direction direction, QWidget* parent = nullptr)
        : QWidget(parent),
          direction_(direction)
    {
        // Nothing
    }

    Direction direction() const { return direction_; }

    void setTimestamp(time_t timestamp) { timestamp_ = timestamp; }
    time_t timestamp() const { return timestamp_; }

    virtual void setMessageText(const QString& text) = 0;
    virtual QString messageText() const = 0;
    virtual QString messageTime() const = 0;

private:
    const Direction direction_;
    time_t timestamp_ = 0;
};

} // namespace common

#endif // COMMON_UI_TEXT_CHAT_MESSAGE_H
