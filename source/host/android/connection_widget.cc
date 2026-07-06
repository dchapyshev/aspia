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

#include <QCoreApplication>
#include <QEvent>
#include <QHBoxLayout>
#include <QJniObject>
#include <QLabel>
#include <QVBoxLayout>
#include <QVariant>

#include "common/android/card.h"
#include "common/android/controls.h"
#include "common/android/icon_button.h"
#include "common/android/label.h"
#include "common/android/switch.h"
#include "common/desktop/session_type.h"
#include "host/user_settings.h"
#include "proto/peer.h"

namespace {

constexpr int kContentMargin = 16;
constexpr int kCardSpacing = 16;
constexpr int kRouterIconSize = 22;
constexpr int kSessionIconSize = 40;

const char kDisplayNameProperty[] = "aspia_display_name";
const char kComputerNameProperty[] = "aspia_computer_name";
const char kShowComputerNameProperty[] = "aspia_show_computer_name";
const char kNameLabelProperty[] = "aspia_name_label";

//--------------------------------------------------------------------------------------------------
// Opens the system share sheet with |text| as plain text (ACTION_SEND).
void shareText(const QString& text)
{
    const QString value = text;
    QNativeInterface::QAndroidApplication::runOnAndroidMainThread([value]() -> QVariant
    {
        QJniObject context = QNativeInterface::QAndroidApplication::context();
        if (!context.isValid())
            return QVariant();

        QJniObject action = QJniObject::getStaticObjectField(
            "android/content/Intent", "ACTION_SEND", "Ljava/lang/String;");
        QJniObject intent("android/content/Intent", "(Ljava/lang/String;)V", action.object());

        QJniObject mime_type = QJniObject::fromString("text/plain");
        intent.callObjectMethod("setType", "(Ljava/lang/String;)Landroid/content/Intent;",
                                mime_type.object());

        QJniObject extra_text_key = QJniObject::getStaticObjectField(
            "android/content/Intent", "EXTRA_TEXT", "Ljava/lang/String;");
        QJniObject jni_text = QJniObject::fromString(value);
        intent.callObjectMethod("putExtra",
            "(Ljava/lang/String;Ljava/lang/String;)Landroid/content/Intent;",
            extra_text_key.object(), jni_text.object());

        QJniObject chooser = QJniObject::callStaticObjectMethod(
            "android/content/Intent", "createChooser",
            "(Landroid/content/Intent;Ljava/lang/CharSequence;)Landroid/content/Intent;",
            intent.object(), static_cast<jobject>(nullptr));

        // The application context is not an activity, so the chooser needs to start its own task.
        const jint flag_new_task = QJniObject::getStaticField<jint>(
            "android/content/Intent", "FLAG_ACTIVITY_NEW_TASK");
        chooser.callObjectMethod("addFlags", "(I)Landroid/content/Intent;", flag_new_task);

        context.callMethod<void>("startActivity", "(Landroid/content/Intent;)V", chooser.object());
        return QVariant();
    });
}

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

    clients_card_ = new Card(content);
    clients_layout_ = clients_card_->contentLayout();
    clients_card_->hide();
    layout->addWidget(clients_card_);

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

    // Lives in the top app bar; AppBar::setActions() reparents and shows it when this section is active.
    share_button_ = new IconButton(":/img/material/share.svg", this);
    share_button_->hide();
    connect(share_button_, &IconButton::clicked, this, &ConnectionWidget::onShare);

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
void ConnectionWidget::setConnectedClients(const QList<Server::ClientInfo>& clients)
{
    // Drop the previous rows.
    while (QLayoutItem* item = clients_layout_->takeAt(0))
    {
        delete item->widget();
        delete item;
    }

    // The card is shown only while at least one client is connected.
    clients_card_->setVisible(!clients.isEmpty());

    for (const Server::ClientInfo& client : clients)
    {
        QWidget* row = new QWidget(clients_card_);

        QHBoxLayout* row_layout = new QHBoxLayout(row);
        row_layout->setContentsMargins(0, 0, 0, 0);
        row_layout->setSpacing(kCardSpacing / 2);

        QLabel* icon = new QLabel(row);
        icon->setFixedSize(kSessionIconSize, kSessionIconSize);
        icon->setPixmap(sessionIcon(client.session_type).pixmap(
            QSize(kSessionIconSize, kSessionIconSize), devicePixelRatioF()));

        const QString primary = !client.display_name.isEmpty() ? client.display_name : client.computer_name;
        Label* name = new Label(primary, Label::Role::BODY, row);
        Label* type = new Label(sessionName(client.session_type), Label::Role::CAPTION, row);

        QVBoxLayout* text_layout = new QVBoxLayout();
        text_layout->setContentsMargins(0, 0, 0, 0);
        text_layout->setSpacing(0);
        text_layout->addWidget(name);
        text_layout->addWidget(type);

        row_layout->addWidget(icon, 0, Qt::AlignVCenter);
        row_layout->addLayout(text_layout, 1);

        row->setProperty(kDisplayNameProperty, client.display_name);
        row->setProperty(kComputerNameProperty, client.computer_name);
        row->setProperty(kNameLabelProperty, QVariant::fromValue<QObject*>(name));
        row->installEventFilter(this);

        clients_layout_->addWidget(row);
    }
}

//--------------------------------------------------------------------------------------------------
QList<QWidget*> ConnectionWidget::appBarActions() const
{
    return { share_button_ };
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
bool ConnectionWidget::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::MouseButtonRelease)
    {
        const QString display = watched->property(kDisplayNameProperty).toString();
        const QString computer = watched->property(kComputerNameProperty).toString();

        // Toggle only when both names are present; otherwise the single one always shows.
        if (!display.isEmpty() && !computer.isEmpty())
        {
            const bool show_computer = !watched->property(kShowComputerNameProperty).toBool();
            watched->setProperty(kShowComputerNameProperty, show_computer);

            auto* name = qobject_cast<Label*>(watched->property(kNameLabelProperty).value<QObject*>());
            if (name)
                name->setText(show_computer ? computer : display);
        }
    }

    return ScrollArea::eventFilter(watched, event);
}

//--------------------------------------------------------------------------------------------------
void ConnectionWidget::onShare()
{
    shareText(tr("Aspia ID: %1\nPassword: %2").arg(host_id_, password_));
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
