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

#include "client/android/router_host_editor.h"

#include <QVBoxLayout>

#include <optional>

#include "base/logging.h"
#include "base/crypto/secure_string.h"
#include "client/config.h"
#include "client/database.h"
#include "common/android/button.h"
#include "common/android/controls.h"
#include "common/android/label.h"
#include "common/android/line_edit.h"
#include "common/android/scroll_area.h"

namespace {

constexpr int kFormMargin = 16;
constexpr int kFormSpacing = 8;

} // namespace

//--------------------------------------------------------------------------------------------------
RouterHostEditor::RouterHostEditor(QWidget* parent)
    : QWidget(parent),
      username_(new LineEdit()),
      password_(new LineEdit()),
      error_(new Label(QString(), Label::Role::CAPTION))
{
    username_->setLabel(tr("User Name"));

    password_->setLabel(tr("Password"));
    password_->setEchoMode(QLineEdit::Password);

    error_->setStyleSheet(QString("color: %1;").arg(Controls::errorColor().name()));
    error_->setWordWrap(true);
    error_->setVisible(false);

    Label* note = new Label(tr("The user name and the password are stored on this device only and "
                               "are not sent to the router. Leave both empty to forget them."),
                            Label::Role::CAPTION);
    note->setWordWrap(true);

    Button* save = new Button(tr("Save"), Button::Role::FILLED);

    QWidget* form = new QWidget();
    QVBoxLayout* form_layout = new QVBoxLayout(form);
    form_layout->setContentsMargins(kFormMargin, kFormMargin, kFormMargin, kFormMargin);
    form_layout->setSpacing(kFormSpacing);
    form_layout->addWidget(error_);
    form_layout->addWidget(username_);
    form_layout->addWidget(password_);
    form_layout->addWidget(note);
    form_layout->addWidget(save);
    form_layout->addStretch();

    ScrollArea* scroll = new ScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setWidget(form);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(scroll);

    connect(save, &Button::clicked, this, &RouterHostEditor::onSaveClicked);
}

//--------------------------------------------------------------------------------------------------
RouterHostEditor::~RouterHostEditor() = default;

//--------------------------------------------------------------------------------------------------
bool RouterHostEditor::prepareForEdit(qint64 router_id, HostId host_id)
{
    // A temporary host id is handed out at random and comes back for another machine, so what was
    // saved under it would be sent to a host the user never gave it to.
    if (router_id <= 0 || host_id == kInvalidHostId || isTempHostId(host_id))
    {
        LOG(ERROR) << "Credentials cannot be kept for host" << host_id << "of router" << router_id;
        return false;
    }

    router_id_ = router_id;
    host_id_ = host_id;

    username_->clear();
    password_->clear();
    error_->setVisible(false);

    std::optional<RouterHostConfig> credentials =
        Database::instance().findRouterHost(router_id_, host_id_);
    if (credentials.has_value())
    {
        username_->setText(credentials->username());
        password_->setText(credentials->password().toString());
    }

    username_->setFocus();
    return true;
}

//--------------------------------------------------------------------------------------------------
void RouterHostEditor::onSaveClicked()
{
    const QString username = username_->text();
    const QString password = password_->text();

    Database& db = Database::instance();

    if (username.isEmpty() && password.isEmpty())
    {
        if (!db.removeRouterHost(router_id_, host_id_))
        {
            showError(tr("Failed to save the credentials."));
            return;
        }

        emit sig_accepted();
        return;
    }

    // The credentials are kept as a pair: an empty pair means the host is not remembered at all.
    if (username.isEmpty() || password.isEmpty())
    {
        showError(tr("Enter both the user name and the password, or leave both empty."));
        return;
    }

    RouterHostConfig credentials;
    credentials.setRouterId(router_id_);
    credentials.setHostId(host_id_);
    credentials.setUsername(username);
    credentials.setPassword(SecureString(password));

    bool saved = false;

    if (db.findRouterHost(router_id_, host_id_).has_value())
        saved = db.modifyRouterHost(credentials);
    else
        saved = db.addRouterHost(credentials);

    if (!saved)
    {
        showError(tr("Failed to save the credentials."));
        return;
    }

    emit sig_accepted();
}

//--------------------------------------------------------------------------------------------------
void RouterHostEditor::showError(const QString& message)
{
    error_->setText(message);
    error_->setVisible(true);
}
