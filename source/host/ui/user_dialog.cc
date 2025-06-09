//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "host/ui/user_dialog.h"

#include <QMessageBox>
#include <QMouseEvent>

#include "base/logging.h"
#include "common/ui/session_type.h"
#include "proto/peer_common.h"

namespace host {

//--------------------------------------------------------------------------------------------------
UserDialog::UserDialog(const base::User& user, const QStringList& exist_names, QWidget* parent)
    : QDialog(parent),
      exist_names_(exist_names),
      user_(user)
{
    LOG(LS_INFO) << "Ctor";
    ui.setupUi(this);

    QPushButton* cancel_button = ui.button_box->button(QDialogButtonBox::StandardButton::Cancel);
    if (cancel_button)
        cancel_button->setText(tr("Cancel"));

    if (user.isValid())
    {
        ui.checkbox_disable_user->setChecked(!(user.flags & base::User::ENABLED));
        ui.edit_username->setText(user.name);

        setAccountChanged(false);
    }
    else
    {
        ui.checkbox_disable_user->setChecked(false);
    }

    auto add_session = [&](const QString& icon, proto::peer::SessionType session_type)
    {
        QTreeWidgetItem* item = new QTreeWidgetItem();

        item->setText(0, common::sessionTypeToLocalizedString(session_type));
        item->setIcon(0, QIcon(icon));
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setData(0, Qt::UserRole, QVariant(session_type));

        if (user.isValid())
        {
            if (user.sessions & static_cast<quint32>(session_type))
                item->setCheckState(0, Qt::Checked);
            else
                item->setCheckState(0, Qt::Unchecked);
        }
        else
        {
            item->setCheckState(0, Qt::Checked);
        }

        ui.tree_sessions->addTopLevelItem(item);
    };

    add_session(":/img/monitor-keyboard.png", proto::peer::SESSION_TYPE_DESKTOP_MANAGE);
    add_session(":/img/monitor.png", proto::peer::SESSION_TYPE_DESKTOP_VIEW);
    add_session(":/img/folder-stand.png", proto::peer::SESSION_TYPE_FILE_TRANSFER);
    add_session(":/img/computer_info.png", proto::peer::SESSION_TYPE_SYSTEM_INFO);
    add_session(":/img/text-chat.png", proto::peer::SESSION_TYPE_TEXT_CHAT);
    add_session(":/img/port-forwarding.png", proto::peer::SESSION_TYPE_PORT_FORWARDING);

    connect(ui.button_check_all, &QPushButton::clicked, this, &UserDialog::onCheckAllButtonPressed);
    connect(ui.button_check_none, &QPushButton::clicked, this, &UserDialog::onCheckNoneButtonPressed);
    connect(ui.button_box, &QDialogButtonBox::clicked, this, &UserDialog::onButtonBoxClicked);

    connect(ui.edit_username, &QLineEdit::textEdited, this, [this]()
    {
        if (!account_changed_)
            setAccountChanged(true);
    });
}

//--------------------------------------------------------------------------------------------------
UserDialog::~UserDialog()
{
    LOG(LS_INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
bool UserDialog::eventFilter(QObject* object, QEvent* event)
{
    if (event->type() == QEvent::MouseButtonDblClick &&
        (object == ui.edit_password || object == ui.edit_password_repeat))
    {
        setAccountChanged(true);

        if (object == ui.edit_password)
            ui.edit_password->setFocus();
        else if (object == ui.edit_password_repeat)
            ui.edit_password_repeat->setFocus();
    }

    return false;
}

//--------------------------------------------------------------------------------------------------
void UserDialog::onCheckAllButtonPressed()
{
    LOG(LS_INFO) << "[ACTION] Check all button pressed";

    for (int i = 0; i < ui.tree_sessions->topLevelItemCount(); ++i)
        ui.tree_sessions->topLevelItem(i)->setCheckState(0, Qt::Checked);
}

//--------------------------------------------------------------------------------------------------
void UserDialog::onCheckNoneButtonPressed()
{
    LOG(LS_INFO) << "[ACTION] Check none button pressed";

    for (int i = 0; i < ui.tree_sessions->topLevelItemCount(); ++i)
        ui.tree_sessions->topLevelItem(i)->setCheckState(0, Qt::Unchecked);
}

//--------------------------------------------------------------------------------------------------
void UserDialog::onButtonBoxClicked(QAbstractButton* button)
{
    if (ui.button_box->standardButton(button) == QDialogButtonBox::Ok)
    {
        LOG(LS_INFO) << "[ACTION] Accepted by user";

        if (account_changed_)
        {
            QString username = ui.edit_username->text();
            QString password = ui.edit_password->text();

            if (!base::User::isValidUserName(username))
            {
                LOG(LS_ERROR) << "Invalid user name: " << username;
                QMessageBox::warning(this,
                                     tr("Warning"),
                                     tr("The user name can not be empty and can contain only alphabet"
                                        " characters, numbers and ""_"", ""-"", ""."", ""@"" characters."),
                                     QMessageBox::Ok);
                ui.edit_username->selectAll();
                ui.edit_username->setFocus();
                return;
            }

            if (exist_names_.contains(username, Qt::CaseInsensitive))
            {
                LOG(LS_ERROR) << "User name already exists: " << username;
                QMessageBox::warning(this,
                                     tr("Warning"),
                                     tr("The username you entered already exists."),
                                     QMessageBox::Ok);
                ui.edit_username->selectAll();
                ui.edit_username->setFocus();
                return;
            }

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
            if (password != ui.edit_password_repeat->text())
#else
            if (password != ui.edit_password_repeat->text().toStdU16String())
#endif
            {
                LOG(LS_ERROR) << "Passwords do not match";
                QMessageBox::warning(this,
                                     tr("Warning"),
                                     tr("The passwords you entered do not match."),
                                     QMessageBox::Ok);
                ui.edit_password->selectAll();
                ui.edit_password->setFocus();
                return;
            }

            if (!base::User::isValidPassword(password))
            {
                LOG(LS_ERROR) << "Invalid password";
                QMessageBox::warning(this,
                                     tr("Warning"),
                                     tr("Password can not be empty and should not exceed %n characters.",
                                        "", base::User::kMaxPasswordLength),
                                     QMessageBox::Ok);
                ui.edit_password->selectAll();
                ui.edit_password->setFocus();
                return;
            }

            if (!base::User::isSafePassword(password))
            {
                QString unsafe =
                    tr("Password you entered does not meet the security requirements!");

                QString safe =
                    tr("The password must contain lowercase and uppercase characters, "
                       "numbers and should not be shorter than %n characters.",
                       "", base::User::kSafePasswordLength);

                QString question = tr("Do you want to enter a different password?");

                QMessageBox message_box(QMessageBox::Warning,
                    tr("Warning"),
                    QString("<b>%1</b><br/>%2<br/>%3").arg(unsafe, safe, question),
                    QMessageBox::Yes | QMessageBox::No,
                    this);
                message_box.button(QMessageBox::Yes)->setText(tr("Yes"));
                message_box.button(QMessageBox::No)->setText(tr("No"));

                if (message_box.exec() == QMessageBox::Yes)
                {
                    ui.edit_password->clear();
                    ui.edit_password_repeat->clear();
                    ui.edit_password->setFocus();
                    return;
                }
            }

            user_ = base::User::create(username, password);
            if (!user_.isValid())
            {
                LOG(LS_ERROR) << "Unable to create user";
                QMessageBox::warning(this,
                                     tr("Warning"),
                                     tr("Unknown internal error when creating or modifying a user."),
                                     QMessageBox::Ok);
                return;
            }
        }

        quint32 sessions = 0;
        for (int i = 0; i < ui.tree_sessions->topLevelItemCount(); ++i)
        {
            QTreeWidgetItem* item = ui.tree_sessions->topLevelItem(i);
            if (item->checkState(0) == Qt::Checked)
                sessions |= item->data(0, Qt::UserRole).toUInt();
        }

        quint32 flags = 0;
        if (!ui.checkbox_disable_user->isChecked())
            flags |= base::User::ENABLED;

        user_.sessions = sessions;
        user_.flags = flags;

        accept();
    }
    else
    {
        LOG(LS_INFO) << "[ACTION] Rejected by user";
        reject();
    }

    close();
}

//--------------------------------------------------------------------------------------------------
void UserDialog::setAccountChanged(bool changed)
{
    LOG(LS_INFO) << "[ACTION] Account changed";
    account_changed_ = changed;

    ui.edit_password->setEnabled(changed);
    ui.edit_password_repeat->setEnabled(changed);

    if (changed)
    {
        ui.edit_password->clear();
        ui.edit_password_repeat->clear();

        Qt::InputMethodHints hints = Qt::ImhHiddenText | Qt::ImhSensitiveData |
            Qt::ImhNoAutoUppercase | Qt::ImhNoPredictiveText;

        ui.edit_password->setEchoMode(QLineEdit::Password);
        ui.edit_password->setInputMethodHints(hints);

        ui.edit_password_repeat->setEchoMode(QLineEdit::Password);
        ui.edit_password_repeat->setInputMethodHints(hints);
    }
    else
    {
        QString text = tr("Double-click to change");

        ui.edit_password->setText(text);
        ui.edit_password_repeat->setText(text);

        ui.edit_password->setEchoMode(QLineEdit::Normal);
        ui.edit_password->setInputMethodHints(Qt::ImhNone);

        ui.edit_password_repeat->setEchoMode(QLineEdit::Normal);
        ui.edit_password_repeat->setInputMethodHints(Qt::ImhNone);

        ui.edit_password->installEventFilter(this);
        ui.edit_password_repeat->installEventFilter(this);
    }
}

} // namespace host
