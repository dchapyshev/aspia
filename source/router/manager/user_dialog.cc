//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "router/manager/user_dialog.h"

#include "base/strings/string_util.h"
#include "base/strings/unicode.h"
#include "peer/user.h"

#include <QAbstractButton>
#include <QMessageBox>

namespace router {

UserDialog::UserDialog(const QVector<proto::User>& users, int user_index, QWidget* parent)
    : QDialog(parent),
      users_(users),
      user_index_(user_index)
{
    ui.setupUi(this);

    if (user_index_ != -1)
    {
        const proto::User& user = users_.at(user_index_);

        ui.checkbox_disable->setChecked(!(user.flags() & peer::User::ENABLED));
        ui.edit_username->setText(QString::fromStdString(user.name()));

        setAccountChanged(false);
    }
    else
    {
        ui.checkbox_disable->setChecked(false);
    }

    auto add_session = [&](proto::RouterSession session_type)
    {
        QTreeWidgetItem* item = new QTreeWidgetItem();

        item->setText(0, sessionTypeToString(session_type));
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setData(0, Qt::UserRole, QVariant(session_type));

        if (user_index_ != -1)
        {
            const proto::User& user = users_.at(user_index_);

            if (user.sessions() & session_type)
                item->setCheckState(0, Qt::Checked);
            else
                item->setCheckState(0, Qt::Unchecked);
        }
        else if (session_type == proto::ROUTER_SESSION_AUTHORIZED_PEER)
        {
            item->setCheckState(0, Qt::Checked);
        }
        else
        {
            item->setCheckState(0, Qt::Unchecked);
        }

        ui.tree_sessions->addTopLevelItem(item);
    };

    add_session(proto::ROUTER_SESSION_AUTHORIZED_PEER);
    add_session(proto::ROUTER_SESSION_MANAGER);

    connect(ui.buttonbox, &QDialogButtonBox::clicked, this, &UserDialog::onButtonBoxClicked);
    connect(ui.edit_username, &QLineEdit::textEdited, [this]()
    {
        setAccountChanged(true);
    });
}

UserDialog::~UserDialog() = default;

const proto::User& UserDialog::user() const
{
    return users_.at(user_index_);
}

bool UserDialog::eventFilter(QObject* object, QEvent* event)
{
    if (event->type() == QEvent::MouseButtonDblClick &&
        (object == ui.edit_password || object == ui.edit_password_retry))
    {
        setAccountChanged(true);

        if (object == ui.edit_password)
            ui.edit_password->setFocus();
        else if (object == ui.edit_password_retry)
            ui.edit_password_retry->setFocus();
    }

    return false;
}

void UserDialog::onButtonBoxClicked(QAbstractButton* button)
{
    QDialogButtonBox::StandardButton standard_button = ui.buttonbox->standardButton(button);
    if (standard_button != QDialogButtonBox::Ok)
    {
        reject();
        close();
        return;
    }

    proto::User* user;
    if (user_index_ == -1)
    {
        // New user.
        users_.push_back(proto::User());
        user = &users_.back();
    }
    else
    {
        // Modify existing.
        user = &users_[user_index_];
    }

    if (account_changed_)
    {
        std::u16string username = ui.edit_username->text().toStdU16String();

        if (!peer::User::isValidUserName(username))
        {
            QMessageBox::warning(this,
                                 tr("Warning"),
                                 tr("The user name can not be empty and can contain only alphabet"
                                    " characters, numbers and ""_"", ""-"", ""."" characters."),
                                 QMessageBox::Ok);

            ui.edit_username->selectAll();
            ui.edit_username->setFocus();
            return;
        }

        for (int i = 0; i < users_.size(); ++i)
        {
            if (i == user_index_)
                continue;

            if (base::compareCaseInsensitive(
                username, base::utf16FromUtf8(users_.at(i).name())) == 0)
            {
                QMessageBox::warning(this,
                                     tr("Warning"),
                                     tr("The username you entered already exists."),
                                     QMessageBox::Ok);

                ui.edit_username->selectAll();
                ui.edit_username->setFocus();
                return;
            }
        }

        if (ui.edit_password->text() != ui.edit_password_retry->text())
        {
            QMessageBox::warning(this,
                                 tr("Warning"),
                                 tr("The passwords you entered do not match."),
                                 QMessageBox::Ok);

            ui.edit_password->selectAll();
            ui.edit_password->setFocus();
            return;
        }

        std::u16string password = ui.edit_password->text().toStdU16String();

        if (!peer::User::isValidPassword(password))
        {
            QMessageBox::warning(this,
                                 tr("Warning"),
                                 tr("Password can not be empty and should not exceed %n characters.",
                                    "", peer::User::kMaxPasswordLength),
                                 QMessageBox::Ok);

            ui.edit_password->selectAll();
            ui.edit_password->setFocus();
            return;
        }

        if (!peer::User::isSafePassword(password))
        {
            QString unsafe =
                tr("Password you entered does not meet the security requirements!");

            QString safe =
                tr("The password must contain lowercase and uppercase characters, "
                   "numbers and should not be shorter than %n characters.",
                   "", peer::User::kSafePasswordLength);

            QString question = tr("Do you want to enter a different password?");

            if (QMessageBox::warning(this,
                                     tr("Warning"),
                                     QString("<b>%1</b><br/>%2<br/>%3")
                                     .arg(unsafe).arg(safe).arg(question),
                                     QMessageBox::Yes,
                                     QMessageBox::No) == QMessageBox::Yes)
            {
                ui.edit_password->clear();
                ui.edit_password_retry->clear();
                ui.edit_password->setFocus();
                return;
            }
        }

        user->set_name(base::utf8FromUtf16(username));
        user->set_password(base::utf8FromUtf16(password));
    }

    uint32_t sessions = 0;
    for (int i = 0; i < ui.tree_sessions->topLevelItemCount(); ++i)
    {
        QTreeWidgetItem* item = ui.tree_sessions->topLevelItem(i);
        if (item->checkState(0) == Qt::Checked)
            sessions |= item->data(0, Qt::UserRole).toInt();
    }

    uint32_t flags = 0;
    if (!ui.checkbox_disable->isChecked())
        flags |= peer::User::ENABLED;

    user->set_sessions(sessions);
    user->set_flags(flags);

    accept();
    close();
}

void UserDialog::setAccountChanged(bool changed)
{
    account_changed_ = changed;

    ui.edit_password->setEnabled(changed);
    ui.edit_password_retry->setEnabled(changed);

    if (changed)
    {
        ui.edit_password->clear();
        ui.edit_password_retry->clear();

        Qt::InputMethodHints hints = Qt::ImhHiddenText | Qt::ImhSensitiveData |
            Qt::ImhNoAutoUppercase | Qt::ImhNoPredictiveText;

        ui.edit_password->setEchoMode(QLineEdit::Password);
        ui.edit_password->setInputMethodHints(hints);

        ui.edit_password_retry->setEchoMode(QLineEdit::Password);
        ui.edit_password_retry->setInputMethodHints(hints);
    }
    else
    {
        QString text = tr("Double-click to change");

        ui.edit_password->setText(text);
        ui.edit_password_retry->setText(text);

        ui.edit_password->setEchoMode(QLineEdit::Normal);
        ui.edit_password->setInputMethodHints(Qt::ImhNone);

        ui.edit_password_retry->setEchoMode(QLineEdit::Normal);
        ui.edit_password_retry->setInputMethodHints(Qt::ImhNone);

        ui.edit_password->installEventFilter(this);
        ui.edit_password_retry->installEventFilter(this);
    }
}

// static
QString UserDialog::sessionTypeToString(proto::RouterSession session_type)
{
    const char* str = nullptr;

    switch (session_type)
    {
        case proto::ROUTER_SESSION_MANAGER:
            str = QT_TR_NOOP("Manager");
            break;

        case proto::ROUTER_SESSION_AUTHORIZED_PEER:
            str = QT_TR_NOOP("Authorized Peer");
            break;

        default:
            break;
    }

    if (!str)
        return QString();

    return tr(str);
}

} // namespace router
