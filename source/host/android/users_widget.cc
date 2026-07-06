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

#include "host/android/users_widget.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QVBoxLayout>
#include <QVector>

#include <utility>

#include "base/gui_application.h"
#include "base/peer/user.h"
#include "common/android/card.h"
#include "common/android/label.h"
#include "host/database.h"

namespace {

constexpr int kContentMargin = 16;
constexpr int kRowSpacing = 8;
constexpr int kAvatarSize = 36;

} // namespace

//--------------------------------------------------------------------------------------------------
UsersWidget::UsersWidget(QWidget* parent)
    : ScrollArea(parent)
{
    buildContent();
}

//--------------------------------------------------------------------------------------------------
UsersWidget::~UsersWidget() = default;

//--------------------------------------------------------------------------------------------------
void UsersWidget::reload()
{
    buildContent();
}

//--------------------------------------------------------------------------------------------------
bool UsersWidget::eventFilter(QObject* object, QEvent* event)
{
    if (event->type() == QEvent::MouseButtonRelease)
    {
        QMouseEvent* mouse_event = static_cast<QMouseEvent*>(event);
        QWidget* card = qobject_cast<QWidget*>(object);

        // Only treat a tap that both pressed and released inside the card as a click.
        if (card && card->rect().contains(mouse_event->pos()))
        {
            emit sig_editUser(card->property("entry_id").toLongLong());
            return true;
        }
    }

    return ScrollArea::eventFilter(object, event);
}

//--------------------------------------------------------------------------------------------------
void UsersWidget::buildContent()
{
    QWidget* content = new QWidget(this);

    QVBoxLayout* layout = new QVBoxLayout(content);
    layout->setContentsMargins(kContentMargin, kContentMargin, kContentMargin, kContentMargin);
    layout->setSpacing(kRowSpacing);

    const QVector<User> users = Database::instance().userList();

    if (users.isEmpty())
    {
        // Centered vertically and horizontally as an empty-state placeholder.
        layout->addStretch();

        Label* empty = new Label(tr("No users"), Label::Role::BODY, content);
        empty->setAlignment(Qt::AlignCenter);
        layout->addWidget(empty);

        layout->addStretch();
    }
    else
    {
        for (const User& user : std::as_const(users))
        {
            Card* card = new Card(content);
            card->setProperty("entry_id", user.entry_id);
            card->setCursor(Qt::PointingHandCursor);
            card->installEventFilter(this);

            QLabel* icon = new QLabel(card);
            icon->setFixedSize(kAvatarSize, kAvatarSize);
            icon->setPixmap(GuiApplication::svgPixmap(
                ":/img/user.svg", QSize(kAvatarSize, kAvatarSize)));

            Label* name = new Label(user.name, Label::Role::TITLE, card);
            const bool enabled = (user.flags & User::ENABLED) != 0;
            Label* status = new Label(enabled ? tr("Enabled") : tr("Disabled"),
                                      Label::Role::CAPTION, card);

            QVBoxLayout* text_column = new QVBoxLayout();
            text_column->setContentsMargins(0, 0, 0, 0);
            text_column->setSpacing(0);
            text_column->addWidget(name);
            text_column->addWidget(status);

            QHBoxLayout* row = new QHBoxLayout();
            row->setContentsMargins(0, 0, 0, 0);
            row->setSpacing(kRowSpacing * 2);
            row->addWidget(icon);
            row->addLayout(text_column, 1);

            card->contentLayout()->addLayout(row);
            layout->addWidget(card);
        }

        layout->addStretch();
    }

    // setWidget() deletes the previously set content widget.
    setWidget(content);
}
