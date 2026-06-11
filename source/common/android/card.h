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

#ifndef COMMON_ANDROID_CARD_H
#define COMMON_ANDROID_CARD_H

#include <QWidget>

class QVBoxLayout;

// Rounded surface container that groups related controls on touch screens, the mobile replacement
// for a titled group box. Child widgets are added to contentLayout().
class Card final : public QWidget
{
    Q_OBJECT

public:
    enum class Role
    {
        FILLED,  // A filled surface that lifts the group off the background.
        OUTLINED // A transparent surface with an outline.
    };

    explicit Card(QWidget* parent = nullptr);
    explicit Card(Role role, QWidget* parent = nullptr);
    ~Card() final;

    void setRole(Role role);
    Role role() const { return role_; }

    QVBoxLayout* contentLayout() const { return content_layout_; }

protected:
    // QWidget implementation.
    void paintEvent(QPaintEvent* event) final;

private:
    QVBoxLayout* content_layout_;
    Role role_;

    Q_DISABLE_COPY_MOVE(Card)
};

#endif // COMMON_ANDROID_CARD_H
