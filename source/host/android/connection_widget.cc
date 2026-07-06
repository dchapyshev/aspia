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

#include "host/android/connection_widget.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

#include "common/android/card.h"
#include "common/android/controls.h"
#include "common/android/icon_button.h"
#include "common/android/label.h"
#include "common/android/switch.h"
#include "host/user_settings.h"
#include "proto/peer.h"

namespace {

constexpr int kContentMargin = 16;
constexpr int kCardSpacing = 16;
constexpr int kRouterIconSize = 22;

} // namespace

//--------------------------------------------------------------------------------------------------
ConnectionWidget::ConnectionWidget(QWidget* parent)
    : ScrollArea(parent)
{
    QWidget* content = new QWidget(this);

    QVBoxLayout* layout = new QVBoxLayout(content);
    layout->setContentsMargins(kContentMargin, kContentMargin, kContentMargin, kContentMargin);
    layout->setSpacing(kCardSpacing);

    // ID card.
    Card* id_card = new Card(content);
    id_caption_ = new Label(QString(), Label::Role::CAPTION, id_card);
    id_value_ = new Label(host_id_, Label::Role::TITLE, id_card);
    id_value_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    id_card->contentLayout()->addWidget(id_caption_);
    id_card->contentLayout()->addWidget(id_value_);
    layout->addWidget(id_card);

    // One-time password card.
    Card* password_card = new Card(content);
    password_caption_ = new Label(QString(), Label::Role::CAPTION, password_card);

    password_value_ = new Label(password_, Label::Role::TITLE, password_card);
    password_value_->setTextInteractionFlags(Qt::TextSelectableByMouse);

    new_password_button_ = new IconButton(":/img/replay.svg", password_card);
    connect(new_password_button_, &IconButton::clicked,
            this, &ConnectionWidget::sig_newPasswordRequested);

    QHBoxLayout* password_row = new QHBoxLayout();
    password_row->setContentsMargins(0, 0, 0, 0);
    password_row->addWidget(password_value_, 1);
    password_row->addWidget(new_password_button_);

    password_card->contentLayout()->addWidget(password_caption_);
    password_card->contentLayout()->addLayout(password_row);
    layout->addWidget(password_card);

    // Access card: the session types an incoming connection is allowed to use. Only the desktop and
    // file transfer sessions are implemented by the Android host. Backed by the one-time session mask
    // in UserSettings; each switch flips only its own bit, leaving the others untouched.
    Card* access_card = new Card(content);
    access_caption_ = new Label(QString(), Label::Role::CAPTION, access_card);
    desktop_session_ = new Switch(access_card);
    file_transfer_session_ = new Switch(access_card);

    const quint32 sessions = UserSettings().oneTimeSessions();

    const auto bind_session = [this](Switch* control, quint32 flag, quint32 current)
    {
        control->setChecked((current & flag) != 0);
        connect(control, &QCheckBox::toggled, this, [flag](bool checked)
        {
            UserSettings settings;
            quint32 mask = settings.oneTimeSessions();

            if (checked)
                mask |= flag;
            else
                mask &= ~flag;

            settings.setOneTimeSessions(mask);
        });
    };

    bind_session(desktop_session_, proto::peer::SESSION_TYPE_DESKTOP, sessions);
    bind_session(file_transfer_session_, proto::peer::SESSION_TYPE_FILE_TRANSFER, sessions);

    access_card->contentLayout()->addWidget(access_caption_);
    access_card->contentLayout()->addWidget(desktop_session_);
    access_card->contentLayout()->addWidget(file_transfer_session_);
    layout->addWidget(access_card);

    // Push the router state to the bottom edge, above the navigation bar.
    layout->addStretch();

    // Router state as a plain status line (not a card): an icon and a wrapping caption.
    router_icon_ = new QLabel(content);
    router_icon_->setFixedSize(kRouterIconSize, kRouterIconSize);

    router_text_ = new Label(QString(), Label::Role::CAPTION, content);
    router_text_->setWordWrap(true);

    QHBoxLayout* router_row = new QHBoxLayout();
    router_row->setContentsMargins(0, 0, 0, 0);
    router_row->setSpacing(kCardSpacing / 2);
    router_row->addWidget(router_icon_, 0, Qt::AlignTop);
    router_row->addWidget(router_text_, 1);

    layout->addLayout(router_row);

    setWidget(content);

    id_caption_->setText(tr("Your ID"));
    password_caption_->setText(tr("One-time password"));
    access_caption_->setText(tr("Access"));
    desktop_session_->setText(tr("Desktop"));
    file_transfer_session_->setText(tr("File Transfer"));
    updateRouterRow();
}

//--------------------------------------------------------------------------------------------------
ConnectionWidget::~ConnectionWidget() = default;

//--------------------------------------------------------------------------------------------------
void ConnectionWidget::setHostId(const QString& host_id)
{
    host_id_ = host_id;
    id_value_->setText(host_id_);
}

//--------------------------------------------------------------------------------------------------
void ConnectionWidget::setPassword(const QString& password)
{
    password_ = password;
    password_value_->setText(password_);
}

//--------------------------------------------------------------------------------------------------
void ConnectionWidget::setRouterState(RouterState state, const QString& router)
{
    router_state_ = state;
    router_ = router;
    updateRouterRow();
}

//--------------------------------------------------------------------------------------------------
void ConnectionWidget::changeEvent(QEvent* event)
{
    ScrollArea::changeEvent(event);

    // The router icon is a tinted pixmap that does not follow the palette on its own; recolor it when
    // the theme changes.
    if (event->type() == QEvent::PaletteChange)
        updateRouterRow();
}

//--------------------------------------------------------------------------------------------------
void ConnectionWidget::updateRouterRow()
{
    QString icon;
    QColor color;

    switch (router_state_)
    {
        case RouterState::DISABLED:
            icon = ":/img/close.svg";
            color = palette().color(QPalette::WindowText);
            break;

        case RouterState::CONNECTING:
            icon = ":/img/replay.svg";
            color = palette().color(QPalette::WindowText);
            break;

        case RouterState::CONNECTED:
            icon = ":/img/done.svg";
            color = Controls::accentColor();
            break;

        case RouterState::FAILED:
            icon = ":/img/close.svg";
            color = Controls::errorColor();
            break;
    }

    router_icon_->setPixmap(Controls::tintedPixmap(
        icon, QSize(kRouterIconSize, kRouterIconSize), color));
    router_text_->setText(routerStatusText());
}

//--------------------------------------------------------------------------------------------------
QString ConnectionWidget::routerStatusText() const
{
    switch (router_state_)
    {
        case RouterState::DISABLED:
            return tr("Router is disabled");

        case RouterState::CONNECTING:
            return router_.isEmpty() ? tr("Connecting to router...")
                                     : tr("Connecting to router %1...").arg(router_);

        case RouterState::CONNECTED:
            return router_.isEmpty() ? tr("Connected to router")
                                     : tr("Connected to router %1").arg(router_);

        case RouterState::FAILED:
            return tr("Failed to connect to router");
    }

    return QString();
}
