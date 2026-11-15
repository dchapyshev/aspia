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

#ifndef CLIENT_ANDROID_ROUTER_CARD_H
#define CLIENT_ANDROID_ROUTER_CARD_H

#include <QList>
#include <QWidget>

#include "client/router/router_types.h"

class Button;
class Card;
class Label;
class QLabel;
class QVBoxLayout;

class RouterCard final : public QWidget
{
    Q_OBJECT

public:
    explicit RouterCard(const QString& name, QWidget* parent = nullptr);
    ~RouterCard() final;

    void setStatus(RouterStatus status);
    void updateTwoFactorButton();

    void setEvents(const QList<RouterEvent>& events);
    void appendEvent(const RouterEvent& event);

signals:
    void sig_twoFactorClicked();

private:
    void clearEvents();

    QLabel* label_status_icon_ = nullptr;
    Label* label_name_ = nullptr;
    Button* button_two_factor_ = nullptr;
    Card* events_card_ = nullptr;
    QVBoxLayout* events_layout_ = nullptr;

    Q_DISABLE_COPY_MOVE(RouterCard)
};

#endif // CLIENT_ANDROID_ROUTER_CARD_H
