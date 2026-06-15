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

#include "client/android/local_host_editor.h"

#include <QVBoxLayout>

#include <optional>

#include "base/crypto/secure_string.h"
#include "base/net/address.h"
#include "base/peer/host_id.h"
#include "base/peer/user.h"
#include "build/build_config.h"
#include "client/config.h"
#include "client/database.h"
#include "common/android/button.h"
#include "common/android/combo_box.h"
#include "common/android/controls.h"
#include "common/android/label.h"
#include "common/android/line_edit.h"
#include "common/android/message_dialog.h"
#include "common/android/scroll_area.h"
#include "common/android/text_area.h"

namespace {

constexpr int kFormMargin = 16;
constexpr int kFormSpacing = 8;

} // namespace

//--------------------------------------------------------------------------------------------------
LocalHostEditor::LocalHostEditor(QWidget* parent)
    : QWidget(parent),
      router_(new ComboBox()),
      name_(new LineEdit()),
      address_(new LineEdit()),
      username_(new LineEdit()),
      password_(new LineEdit()),
      comment_(new TextArea()),
      error_(new Label(QString(), Label::Role::CAPTION))
{
    name_->setLabel(tr("Name"));
    router_->setLabel(tr("Router"));
    address_->setLabel(tr("Address"));
    username_->setLabel(tr("User Name"));
    password_->setLabel(tr("Password"));
    password_->setEchoMode(QLineEdit::Password);
    comment_->setLabel(tr("Comment"));

    // A fixed hex keeps the error color readable on both light and dark surfaces and survives the
    // palette reset that the caption role applies on theme changes.
    error_->setStyleSheet(QString("color: %1;").arg(Controls::errorColor().name()));
    error_->setWordWrap(true);
    error_->setVisible(false);

    Button* save = new Button(tr("Save"), Button::Role::FILLED);

    // The delete action is destructive, so its text is tinted red and it shows only when editing.
    delete_button_ = new Button(tr("Delete"), Button::Role::TEXT);
    delete_button_->setAccentColor(Controls::errorColor());
    delete_button_->hide();

    QWidget* form = new QWidget();
    QVBoxLayout* form_layout = new QVBoxLayout(form);
    form_layout->setContentsMargins(kFormMargin, kFormMargin, kFormMargin, kFormMargin);
    form_layout->setSpacing(kFormSpacing);
    form_layout->addWidget(error_);
    form_layout->addWidget(name_);
    form_layout->addWidget(router_);
    form_layout->addWidget(address_);
    form_layout->addWidget(username_);
    form_layout->addWidget(password_);
    form_layout->addWidget(comment_);
    form_layout->addWidget(save);
    form_layout->addWidget(delete_button_);
    form_layout->addStretch();

    ScrollArea* scroll = new ScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setWidget(form);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(scroll);

    connect(router_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &LocalHostEditor::onRouterChanged);
    connect(save, &Button::clicked, this, &LocalHostEditor::onSaveClicked);
    connect(delete_button_, &Button::clicked, this, &LocalHostEditor::onDeleteClicked);
}

//--------------------------------------------------------------------------------------------------
LocalHostEditor::~LocalHostEditor() = default;

//--------------------------------------------------------------------------------------------------
void LocalHostEditor::prepareForAdd(qint64 group_id)
{
    entry_id_ = -1;
    group_id_ = group_id;

    name_->clear();
    address_->clear();
    username_->clear();
    password_->clear();
    comment_->clear();
    error_->setVisible(false);
    delete_button_->hide();

    loadRouters(0);
    onRouterChanged();
    name_->setFocus();
}

//--------------------------------------------------------------------------------------------------
void LocalHostEditor::prepareForEdit(qint64 host_id)
{
    std::optional<HostConfig> host = Database::instance().findHost(host_id);
    if (!host.has_value())
        return;

    entry_id_ = host_id;
    group_id_ = host->groupId();

    name_->setText(host->name());
    address_->setText(host->address());
    username_->setText(host->username());
    password_->setText(host->password().toString());
    comment_->setText(host->comment());
    error_->setVisible(false);
    delete_button_->show();

    loadRouters(host->routerId());
    onRouterChanged();
    name_->setFocus();
}

//--------------------------------------------------------------------------------------------------
void LocalHostEditor::loadRouters(qint64 selected_router_id)
{
    router_->clear();
    router_->addItem(tr("Without Router"), QVariant::fromValue<qint64>(0));
    for (const RouterConfig& router : Database::instance().routerList())
        router_->addItem(router.displayLabel(), QVariant::fromValue(router.routerId()));

    const int index = router_->findData(QVariant::fromValue(selected_router_id));
    router_->setCurrentIndex(index >= 0 ? index : 0);
}

//--------------------------------------------------------------------------------------------------
void LocalHostEditor::onRouterChanged()
{
    // Without a router the address is a host name or IP; through a router it is a host ID.
    const bool direct = (router_->currentData().toLongLong() == 0);
    address_->setLabel(direct ? tr("Address") : tr("ID"));
}

//--------------------------------------------------------------------------------------------------
void LocalHostEditor::onSaveClicked()
{
    const QString name = name_->text();
    if (name.isEmpty())
    {
        showError(tr("Name cannot be empty."));
        name_->setFocus();
        return;
    }

    const qint64 router_id = router_->currentData().toLongLong();
    const QString address_text = address_->text();

    if (router_id == 0)
    {
        if (!Address::fromString(address_text, DEFAULT_HOST_TCP_PORT).isValid())
        {
            showError(tr("An invalid host address was entered."));
            address_->setFocus();
            address_->selectAll();
            return;
        }
    }
    else if (!isHostId(address_text))
    {
        showError(tr("An invalid host ID was entered."));
        address_->setFocus();
        address_->selectAll();
        return;
    }

    const QString username = username_->text();
    if (!username.isEmpty() && !User::isValidUserName(username))
    {
        showError(tr("The user name can not be empty and can contain only alphabet characters,"
                     " numbers and \"_\", \"-\", \".\" characters."));
        username_->setFocus();
        username_->selectAll();
        return;
    }

    HostConfig data;
    data.setId(entry_id_);
    data.setGroupId(group_id_);
    data.setRouterId(router_id);
    data.setName(name);
    data.setAddress(address_text);
    data.setUsername(username);
    data.setPassword(SecureString(password_->text()));
    data.setComment(comment_->text());

    Database& db = Database::instance();
    const bool saved = (entry_id_ < 0) ? db.addHost(data) : db.modifyHost(data);
    if (!saved)
    {
        showError(tr("Failed to save the host."));
        return;
    }

    emit sig_accepted();
}

//--------------------------------------------------------------------------------------------------
void LocalHostEditor::onDeleteClicked()
{
    if (!MessageDialog::confirm(this, tr("Delete Host"),
                                tr("Delete the host \"%1\"?").arg(name_->text()), tr("Delete")))
    {
        return;
    }

    if (!Database::instance().removeHost(entry_id_))
    {
        showError(tr("Failed to delete the host."));
        return;
    }

    emit sig_accepted();
}

//--------------------------------------------------------------------------------------------------
void LocalHostEditor::showError(const QString& message)
{
    error_->setText(message);
    error_->setVisible(true);
}
