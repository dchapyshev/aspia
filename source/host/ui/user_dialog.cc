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

#include "host/ui/user_dialog.h"

#include "base/logging.h"
#include "common/session_type.h"
#include "common/user_util.h"
#include "proto/common.pb.h"

#include <QMessageBox>
#include <QMouseEvent>

namespace host {

UserDialog::UserDialog(const User& user, const QStringList& exist_names, QWidget* parent)
    : QDialog(parent),
      exist_names_(exist_names),
      user_(user)
{
    ui.setupUi(this);

    if (user.isValid())
    {
        ui.checkbox_disable_user->setChecked(!(user.flags & User::ENABLED));
        ui.edit_username->setText(QString::fromStdU16String(user.name));

        setAccountChanged(false);
    }
    else
    {
        ui.checkbox_disable_user->setChecked(false);
    }

    auto add_session = [&](const QString& icon, proto::SessionType session_type)
    {
        QTreeWidgetItem* item = new QTreeWidgetItem();

        item->setText(0, common::sessionTypeToLocalizedString(session_type));
        item->setIcon(0, QIcon(icon));
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setData(0, Qt::UserRole, QVariant(session_type));

        if (user.isValid())
        {
            if (user.sessions & session_type)
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

    add_session(QStringLiteral(":/img/monitor-keyboard.png"), proto::SESSION_TYPE_DESKTOP_MANAGE);
    add_session(QStringLiteral(":/img/monitor.png"), proto::SESSION_TYPE_DESKTOP_VIEW);
    add_session(QStringLiteral(":/img/folder-stand.png"), proto::SESSION_TYPE_FILE_TRANSFER);

    connect(ui.button_check_all, &QPushButton::released, this, &UserDialog::onCheckAllButtonPressed);
    connect(ui.button_check_none, &QPushButton::released, this, &UserDialog::onCheckNoneButtonPressed);
    connect(ui.button_box, &QDialogButtonBox::clicked, this, &UserDialog::onButtonBoxClicked);

    connect(ui.edit_username, &QLineEdit::textEdited, [this]()
    {
        setAccountChanged(true);
    });
}

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

void UserDialog::onCheckAllButtonPressed()
{
    for (int i = 0; i < ui.tree_sessions->topLevelItemCount(); ++i)
        ui.tree_sessions->topLevelItem(i)->setCheckState(0, Qt::Checked);
}

void UserDialog::onCheckNoneButtonPressed()
{
    for (int i = 0; i < ui.tree_sessions->topLevelItemCount(); ++i)
        ui.tree_sessions->topLevelItem(i)->setCheckState(0, Qt::Unchecked);
}

void UserDialog::onButtonBoxClicked(QAbstractButton* button)
{
    if (ui.button_box->standardButton(button) == QDialogButtonBox::Ok)
    {
        if (account_changed_)
        {
            std::u16string name = ui.edit_username->text().toStdU16String();
            std::u16string password = ui.edit_password->text().toStdU16String();

            if (!common::UserUtil::isValidUserName(name))
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

            if (exist_names_.contains(name, Qt::CaseInsensitive))
            {
                QMessageBox::warning(this,
                                     tr("Warning"),
                                     tr("The username you entered already exists."),
                                     QMessageBox::Ok);
                ui.edit_username->selectAll();
                ui.edit_username->setFocus();
                return;
            }

            if (password != ui.edit_password_repeat->text())
            {
                QMessageBox::warning(this,
                                     tr("Warning"),
                                     tr("The passwords you entered do not match."),
                                     QMessageBox::Ok);
                ui.edit_password->selectAll();
                ui.edit_password->setFocus();
                return;
            }

            if (!common::UserUtil::isValidPassword(password))
            {
                QMessageBox::warning(this,
                                     tr("Warning"),
                                     tr("Password can not be empty and should not exceed %n characters.",
                                        "", common::UserUtil::kMaxPasswordLength),
                                     QMessageBox::Ok);
                ui.edit_password->selectAll();
                ui.edit_password->setFocus();
                return;
            }

            if (!common::UserUtil::isSafePassword(password))
            {
                QString unsafe =
                    tr("Password you entered does not meet the security requirements!");

                QString safe =
                    tr("The password must contain lowercase and uppercase characters, "
                       "numbers and should not be shorter than %n characters.",
                       "", common::UserUtil::kSafePasswordLength);

                QString question = tr("Do you want to enter a different password?");

                if (QMessageBox::warning(this,
                                         tr("Warning"),
                                         QString("<b>%1</b><br/>%2<br/>%3")
                                             .arg(unsafe).arg(safe).arg(question),
                                         QMessageBox::Yes,
                                         QMessageBox::No) == QMessageBox::Yes)
                {
                    ui.edit_password->clear();
                    ui.edit_password_repeat->clear();
                    ui.edit_password->setFocus();
                    return;
                }
            }

            user_ = User::create(name, password);
            if (!user_.isValid())
            {
                QMessageBox::warning(this,
                                     tr("Warning"),
                                     tr("Unknown internal error when creating or modifying a user."),
                                     QMessageBox::Ok);
                return;
            }
        }

        uint32_t sessions = 0;
        for (int i = 0; i < ui.tree_sessions->topLevelItemCount(); ++i)
        {
            QTreeWidgetItem* item = ui.tree_sessions->topLevelItem(i);
            if (item->checkState(0) == Qt::Checked)
                sessions |= item->data(0, Qt::UserRole).toInt();
        }

        uint32_t flags = 0;
        if (!ui.checkbox_disable_user->isChecked())
            flags |= User::ENABLED;

        user_.sessions = sessions;
        user_.flags = flags;

        accept();
    }
    else
    {
        reject();
    }

    close();
}

void UserDialog::setAccountChanged(bool changed)
{
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
