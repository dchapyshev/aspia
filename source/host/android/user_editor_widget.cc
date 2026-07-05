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

#include "host/android/user_editor_widget.h"

#include <QLineEdit>
#include <QVBoxLayout>
#include <QVector>

#include <utility>

#include "base/crypto/random.h"
#include "base/crypto/secure_string.h"
#include "common/android/button.h"
#include "common/android/controls.h"
#include "common/android/label.h"
#include "common/android/line_edit.h"
#include "common/android/message_dialog.h"
#include "common/android/switch.h"
#include "host/database.h"
#include "proto/peer.h"

namespace {

constexpr int kContentMargin = 16;
constexpr int kRowSpacing = 8;
constexpr int kSectionSpacing = 24;
constexpr int kSeedKeySize = 64;

} // namespace

//--------------------------------------------------------------------------------------------------
UserEditorWidget::UserEditorWidget(QWidget* parent)
    : ScrollArea(parent)
{
    QWidget* content = new QWidget(this);

    QVBoxLayout* layout = new QVBoxLayout(content);
    layout->setContentsMargins(kContentMargin, kContentMargin, kContentMargin, kContentMargin);
    layout->setSpacing(kRowSpacing);

    username_ = new LineEdit();

    password_ = new LineEdit();
    password_->setEchoMode(QLineEdit::Password);

    password_repeat_ = new LineEdit();
    password_repeat_->setEchoMode(QLineEdit::Password);

    password_hint_ = new Label(QString(), Label::Role::CAPTION);
    password_hint_->setWordWrap(true);

    enabled_ = new Switch();

    sessions_header_ = new Label(QString(), Label::Role::CAPTION);

    desktop_ = new Switch();
    file_transfer_ = new Switch();

    // Only shown while editing an existing user (see load()).
    delete_button_ = new Button(QString(), Button::Role::TEXT);
    delete_button_->setAccentColor(Controls::errorColor());
    delete_button_->hide();
    connect(delete_button_, &Button::clicked, this, &UserEditorWidget::deleteUser);

    layout->addWidget(username_);
    layout->addWidget(password_);
    layout->addWidget(password_repeat_);
    layout->addWidget(password_hint_);
    layout->addWidget(enabled_);
    layout->addSpacing(kSectionSpacing);
    layout->addWidget(sessions_header_);
    layout->addWidget(desktop_);
    layout->addWidget(file_transfer_);
    layout->addStretch();
    layout->addWidget(delete_button_);

    setWidget(content);

    retranslate();
}

//--------------------------------------------------------------------------------------------------
UserEditorWidget::~UserEditorWidget() = default;

//--------------------------------------------------------------------------------------------------
void UserEditorWidget::load(qint64 entry_id)
{
    entry_id_ = entry_id;

    if (entry_id_ != 0)
    {
        user_ = Database::instance().findUser(entry_id_);

        username_->setText(user_.name);
        enabled_->setChecked((user_.flags & User::ENABLED) != 0);
        desktop_->setChecked((user_.sessions & proto::peer::SESSION_TYPE_DESKTOP) != 0);
        file_transfer_->setChecked((user_.sessions & proto::peer::SESSION_TYPE_FILE_TRANSFER) != 0);
    }
    else
    {
        user_ = User();

        username_->clear();
        enabled_->setChecked(true);
        desktop_->setChecked(true);
        file_transfer_->setChecked(true);
    }

    // Editing keeps the current password when the fields are left empty. The hint says so (a
    // placeholder would overlap the floating field label); it is irrelevant for a new user.
    password_hint_->setVisible(entry_id_ != 0);
    delete_button_->setVisible(entry_id_ != 0);
    password_->clear();
    password_repeat_->clear();
}

//--------------------------------------------------------------------------------------------------
void UserEditorWidget::save()
{
    Database& db = Database::instance();

    const QString username = username_->text().trimmed();

    if (!User::isValidUserName(username))
    {
        MessageDialog::info(this, tr("Error"),
            tr("The user name can not be empty and can contain only alphabet characters, numbers "
               "and \"_\", \"-\", \".\", \"@\" characters."));
        username_->setFocus();
        return;
    }

    const QVector<User> users = db.userList();
    for (const User& existing : std::as_const(users))
    {
        if (existing.entry_id != entry_id_ &&
            existing.name.compare(username, Qt::CaseInsensitive) == 0)
        {
            MessageDialog::info(this, tr("Error"), tr("The username you entered already exists."));
            username_->setFocus();
            return;
        }
    }

    User user = user_;

    // A new user always needs a password; an existing one only when the password was entered or the
    // name changed (both require the SRP verifier to be rebuilt).
    const bool need_password =
        entry_id_ == 0 || !password_->text().isEmpty() || username != user_.name;

    if (need_password)
    {
        const SecureString password(password_->text());
        const SecureString password_repeat(password_repeat_->text());

        if (password != password_repeat)
        {
            MessageDialog::info(this, tr("Error"), tr("The passwords you entered do not match."));
            password_->setFocus();
            return;
        }

        if (!User::isValidPassword(password))
        {
            MessageDialog::info(this, tr("Error"),
                tr("Password can not be empty and should not exceed %n characters.",
                   "", static_cast<int>(User::kMaxPasswordLength)));
            password_->setFocus();
            return;
        }

        user = User::create(username, password);
        if (!user.isValid())
        {
            MessageDialog::info(this, tr("Error"),
                tr("Unknown internal error when creating or modifying a user."));
            return;
        }
    }

    quint32 sessions = 0;
    if (desktop_->isChecked())
        sessions |= proto::peer::SESSION_TYPE_DESKTOP;
    if (file_transfer_->isChecked())
        sessions |= proto::peer::SESSION_TYPE_FILE_TRANSFER;

    user.name = username;
    user.sessions = sessions;
    user.flags = enabled_->isChecked() ? User::ENABLED : 0;

    if (entry_id_ == 0)
    {
        if (db.seedKey().isEmpty())
            db.setSeedKey(Random::byteArray(kSeedKeySize));

        if (!db.addUser(user))
        {
            MessageDialog::info(this, tr("Error"),
                tr("Unknown internal error when creating or modifying a user."));
            return;
        }
    }
    else
    {
        user.entry_id = entry_id_;

        if (!db.modifyUser(user))
        {
            MessageDialog::info(this, tr("Error"),
                tr("Unknown internal error when creating or modifying a user."));
            return;
        }
    }

    emit sig_saved();
}

//--------------------------------------------------------------------------------------------------
void UserEditorWidget::retranslate()
{
    username_->setLabel(tr("User name"));
    password_->setLabel(tr("Password"));
    password_repeat_->setLabel(tr("Password (repeat)"));
    password_hint_->setText(tr("Leave the password empty to keep the current one."));
    enabled_->setText(tr("User enabled"));
    sessions_header_->setText(tr("Allowed sessions"));
    desktop_->setText(tr("Desktop"));
    file_transfer_->setText(tr("File Transfer"));
    delete_button_->setText(tr("Delete user"));
}

//--------------------------------------------------------------------------------------------------
void UserEditorWidget::deleteUser()
{
    if (entry_id_ == 0)
        return;

    if (!MessageDialog::confirm(this, tr("Delete User"),
                                tr("Delete the user \"%1\"?").arg(user_.name), tr("Delete")))
    {
        return;
    }

    if (!Database::instance().removeUser(entry_id_))
    {
        MessageDialog::info(this, tr("Error"), tr("Failed to delete the user."));
        return;
    }

    emit sig_saved();
}
