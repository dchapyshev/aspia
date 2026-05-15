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

#include "host/ui/user_dialog.h"

#include <QMouseEvent>

#include "base/logging.h"
#include "base/crypto/random.h"
#include "base/crypto/secure_string.h"
#include "common/ui/msg_box.h"
#include "common/ui/password_edit.h"
#include "common/ui/session_type.h"
#include "host/database.h"
#include "proto/peer.h"
#include "ui_user_dialog.h"

namespace {

const int kSeedKeySize = 64;

} // namespace

//--------------------------------------------------------------------------------------------------
UserDialog::UserDialog(qint64 entry_id, QWidget* parent)
    : QDialog(parent),
      ui(std::make_unique<Ui::UserDialog>()),
      entry_id_(entry_id)
{
    LOG(INFO) << "Ctor";
    ui->setupUi(this);

    if (entry_id_ != 0)
        user_ = Database::instance().findUser(entry_id_);

    if (user_.isValid())
    {
        ui->checkbox_disable_user->setChecked(!(user_.flags & User::ENABLED));
        ui->edit_username->setText(user_.name);

        setAccountChanged(false);
    }
    else
    {
        ui->checkbox_disable_user->setChecked(false);
    }

    auto add_session = [&](proto::peer::SessionType session_type)
    {
        QTreeWidgetItem* item = new QTreeWidgetItem();

        item->setText(0, sessionName(session_type));
        item->setIcon(0, sessionIcon(session_type));
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setData(0, Qt::UserRole, QVariant(session_type));

        if (user_.isValid())
        {
            if (user_.sessions & static_cast<quint32>(session_type))
                item->setCheckState(0, Qt::Checked);
            else
                item->setCheckState(0, Qt::Unchecked);
        }
        else
        {
            item->setCheckState(0, Qt::Checked);
        }

        ui->tree_sessions->addTopLevelItem(item);
    };

    add_session(proto::peer::SESSION_TYPE_DESKTOP);
    add_session(proto::peer::SESSION_TYPE_FILE_TRANSFER);
    add_session(proto::peer::SESSION_TYPE_SYSTEM_INFO);
    add_session(proto::peer::SESSION_TYPE_CHAT);

    connect(ui->button_check_all, &QPushButton::clicked, this, &UserDialog::onCheckAllButtonPressed);
    connect(ui->button_check_none, &QPushButton::clicked, this, &UserDialog::onCheckNoneButtonPressed);
    connect(ui->button_box, &QDialogButtonBox::clicked, this, &UserDialog::onButtonBoxClicked);

    connect(ui->edit_username, &QLineEdit::textEdited, this, [this]()
    {
        if (!account_changed_)
            setAccountChanged(true);
    });
}

//--------------------------------------------------------------------------------------------------
UserDialog::~UserDialog()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
bool UserDialog::eventFilter(QObject* object, QEvent* event)
{
    if (event->type() == QEvent::MouseButtonDblClick &&
        (object == ui->edit_password || object == ui->edit_password_repeat))
    {
        setAccountChanged(true);

        if (object == ui->edit_password)
            ui->edit_password->setFocus();
        else if (object == ui->edit_password_repeat)
            ui->edit_password_repeat->setFocus();
    }

    return false;
}

//--------------------------------------------------------------------------------------------------
void UserDialog::onCheckAllButtonPressed()
{
    LOG(INFO) << "[ACTION] Check all button pressed";

    for (int i = 0; i < ui->tree_sessions->topLevelItemCount(); ++i)
        ui->tree_sessions->topLevelItem(i)->setCheckState(0, Qt::Checked);
}

//--------------------------------------------------------------------------------------------------
void UserDialog::onCheckNoneButtonPressed()
{
    LOG(INFO) << "[ACTION] Check none button pressed";

    for (int i = 0; i < ui->tree_sessions->topLevelItemCount(); ++i)
        ui->tree_sessions->topLevelItem(i)->setCheckState(0, Qt::Unchecked);
}

//--------------------------------------------------------------------------------------------------
void UserDialog::onButtonBoxClicked(QAbstractButton* button)
{
    if (ui->button_box->standardButton(button) == QDialogButtonBox::Ok)
    {
        LOG(INFO) << "[ACTION] Accepted by user";

        if (account_changed_)
        {
            QString username = ui->edit_username->text();
            SecureString password = ui->edit_password->password();
            SecureString password_repeat = ui->edit_password_repeat->password();

            if (!User::isValidUserName(username))
            {
                LOG(ERROR) << "Invalid user name:" << username;
                MsgBox::warning(this,
                    tr("The user name can not be empty and can contain only alphabet"
                       " characters, numbers and ""_"", ""-"", ""."", ""@"" characters."));
                ui->edit_username->selectAll();
                ui->edit_username->setFocus();
                return;
            }

            QStringList exist_names;
            for (const User& existing : Database::instance().userList())
            {
                if (existing.entry_id != entry_id_)
                    exist_names.append(existing.name);
            }

            if (exist_names.contains(username, Qt::CaseInsensitive))
            {
                LOG(ERROR) << "User name already exists:" << username;
                MsgBox::warning(this, tr("The username you entered already exists."));
                ui->edit_username->selectAll();
                ui->edit_username->setFocus();
                return;
            }

            if (password != password_repeat)
            {
                LOG(ERROR) << "Passwords do not match";
                MsgBox::warning(this, tr("The passwords you entered do not match."));
                ui->edit_password->selectAll();
                ui->edit_password->setFocus();
                return;
            }

            if (!User::isValidPassword(password))
            {
                LOG(ERROR) << "Invalid password";
                MsgBox::warning(this,
                    tr("Password can not be empty and should not exceed %n characters.",
                       "", User::kMaxPasswordLength));
                ui->edit_password->selectAll();
                ui->edit_password->setFocus();
                return;
            }

            if (!User::isSafePassword(password))
            {
                QString unsafe =
                    tr("Password you entered does not meet the security requirements!");

                QString safe =
                    tr("The password must contain lowercase and uppercase characters, "
                       "numbers and should not be shorter than %n characters.",
                       "", User::kSafePasswordLength);

                QString question = tr("Do you want to enter a different password?");

                MsgBox message_box(MsgBox::Warning,
                    tr("Warning"),
                    QString("<b>%1</b><br/>%2<br/>%3").arg(unsafe, safe, question),
                    MsgBox::Yes | MsgBox::No,
                    this);

                if (message_box.exec() == MsgBox::Yes)
                {
                    ui->edit_password->clear();
                    ui->edit_password_repeat->clear();
                    ui->edit_password->setFocus();
                    return;
                }
            }

            user_ = User::create(username, password);
            if (!user_.isValid())
            {
                LOG(ERROR) << "Unable to create user";
                MsgBox::warning(this, tr("Unknown internal error when creating or modifying a user."));
                return;
            }
        }

        quint32 sessions = 0;
        for (int i = 0; i < ui->tree_sessions->topLevelItemCount(); ++i)
        {
            QTreeWidgetItem* item = ui->tree_sessions->topLevelItem(i);
            if (item->checkState(0) == Qt::Checked)
                sessions |= item->data(0, Qt::UserRole).toUInt();
        }

        quint32 flags = 0;
        if (!ui->checkbox_disable_user->isChecked())
            flags |= User::ENABLED;

        user_.sessions = sessions;
        user_.flags = flags;

        Database& db = Database::instance();

        if (entry_id_ == 0)
        {
            if (db.seedKey().isEmpty())
                db.setSeedKey(Random::byteArray(kSeedKeySize));

            if (!db.addUser(user_))
            {
                LOG(ERROR) << "Unable to add user to database";
                MsgBox::warning(this, tr("Unknown internal error when creating or modifying a user."));
                return;
            }
        }
        else
        {
            user_.entry_id = entry_id_;
            if (!db.modifyUser(user_))
            {
                LOG(ERROR) << "Unable to modify user in database";
                MsgBox::warning(this, tr("Unknown internal error when creating or modifying a user."));
                return;
            }
        }

        accept();
    }
    else
    {
        LOG(INFO) << "[ACTION] Rejected by user";
        reject();
    }

    close();
}

//--------------------------------------------------------------------------------------------------
void UserDialog::setAccountChanged(bool changed)
{
    LOG(INFO) << "[ACTION] Account changed";
    account_changed_ = changed;

    ui->edit_password->setEnabled(changed);
    ui->edit_password_repeat->setEnabled(changed);

    ui->edit_password->clear();
    ui->edit_password_repeat->clear();

    ui->edit_password->setShowPassword(!changed);
    ui->edit_password_repeat->setShowPassword(!changed);

    if (changed)
    {
        ui->edit_password->setPlaceholderText(QString());
        ui->edit_password_repeat->setPlaceholderText(QString());
    }
    else
    {
        QString prompt = tr("Double-click to change");
        ui->edit_password->setPlaceholderText(prompt);
        ui->edit_password_repeat->setPlaceholderText(prompt);

        ui->edit_password->installEventFilter(this);
        ui->edit_password_repeat->installEventFilter(this);
    }
}
