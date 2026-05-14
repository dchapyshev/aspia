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

#include "client/ui/hosts/router_user_dialog.h"

#include <QAbstractButton>
#include <QPushButton>

#include "base/logging.h"
#include "base/crypto/secure_string.h"
#include "common/ui/msg_box.h"
#include "common/ui/password_edit.h"
#include "proto/router.h"
#include "ui_router_user_dialog.h"

//--------------------------------------------------------------------------------------------------
RouterUserDialog::RouterUserDialog(const User& user, const QStringList& users, QWidget* parent)
    : QDialog(parent),
      ui(std::make_unique<Ui::RouterUserDialog>()),
      user_(user),
      users_(users)
{
    LOG(INFO) << "Ctor";
    ui->setupUi(this);

    if (user_.isValid())
    {
        ui->checkbox_disable->setChecked(!(user_.flags & User::ENABLED));
        ui->edit_username->setText(user_.name);

        setAccountChanged(false);
    }
    else
    {
        ui->checkbox_disable->setChecked(false);
    }

    auto add_session = [&](proto::router::SessionType session_type)
    {
        QTreeWidgetItem* item = new QTreeWidgetItem();

        item->setText(0, sessionTypeToString(session_type));
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setData(0, Qt::UserRole, QVariant(session_type));

        if (user_.isValid())
        {
            if (user_.sessions & static_cast<quint32>(session_type))
                item->setCheckState(0, Qt::Checked);
            else
                item->setCheckState(0, Qt::Unchecked);
        }
        else if (session_type == proto::router::SESSION_TYPE_CLIENT)
        {
            item->setCheckState(0, Qt::Checked);
        }
        else
        {
            item->setCheckState(0, Qt::Unchecked);
        }

        ui->tree_sessions->addTopLevelItem(item);
    };

    add_session(proto::router::SESSION_TYPE_ADMIN);
    add_session(proto::router::SESSION_TYPE_MANAGER);
    add_session(proto::router::SESSION_TYPE_CLIENT);

    connect(ui->buttonbox, &QDialogButtonBox::clicked, this, &RouterUserDialog::onButtonBoxClicked);
    connect(ui->edit_username, &QLineEdit::textEdited, this, [this]()
    {
        setAccountChanged(true);
    });
}

//--------------------------------------------------------------------------------------------------
RouterUserDialog::~RouterUserDialog()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
const User& RouterUserDialog::user() const
{
    return user_;
}

//--------------------------------------------------------------------------------------------------
bool RouterUserDialog::eventFilter(QObject* object, QEvent* event)
{
    if (event->type() == QEvent::MouseButtonDblClick &&
        (object == ui->edit_password || object == ui->edit_password_retry))
    {
        setAccountChanged(true);

        if (object == ui->edit_password)
            ui->edit_password->setFocus();
        else if (object == ui->edit_password_retry)
            ui->edit_password_retry->setFocus();
    }

    return false;
}

//--------------------------------------------------------------------------------------------------
void RouterUserDialog::onButtonBoxClicked(QAbstractButton* button)
{
    QDialogButtonBox::StandardButton standard_button = ui->buttonbox->standardButton(button);
    if (standard_button != QDialogButtonBox::Ok)
    {
        LOG(INFO) << "[ACTION] Action rejected";
        reject();
        close();
        return;
    }

    if (account_changed_)
    {
        QString username = ui->edit_username->text();

        if (!User::isValidUserName(username))
        {
            LOG(ERROR) << "Invalid user name:" << username;
            MsgBox::warning(this, tr("The user name can not be empty and can contain only "
                "alphabet characters, numbers and ""_"", ""-"", ""."", ""@"" characters."));
            ui->edit_username->selectAll();
            ui->edit_username->setFocus();
            return;
        }

        for (QStringList::size_type i = 0; i < users_.size(); ++i)
        {
            if (username.compare(users_.at(i), Qt::CaseInsensitive) == 0)
            {
                LOG(ERROR) << "User name already exists:" << username;
                MsgBox::warning(this, tr("The username you entered already exists."));
                ui->edit_username->selectAll();
                ui->edit_username->setFocus();
                return;
            }
        }

        SecureString password = ui->edit_password->password();

        if (password != ui->edit_password_retry->password())
        {
            LOG(INFO) << "Passwords do not match";
            MsgBox::warning(this, tr("The passwords you entered do not match."));
            ui->edit_password->selectAll();
            ui->edit_password->setFocus();
            return;
        }

        if (!User::isValidPassword(password))
        {
            LOG(INFO) << "Invalid password";
            MsgBox::warning(this, tr("Password can not be empty and should not exceed %n characters.",
                "", User::kMaxPasswordLength));

            ui->edit_password->selectAll();
            ui->edit_password->setFocus();
            return;
        }

        if (!User::isSafePassword(password))
        {
            QString unsafe = tr("Password you entered does not meet the security requirements!");
            QString safe = tr("The password must contain lowercase and uppercase characters, "
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
                ui->edit_password_retry->clear();
                ui->edit_password->setFocus();
                return;
            }
        }

        // Save entry ID.
        qint64 entry_id = user_.entry_id;

        // Create new user.
        user_ = User::create(username, password);

        // Restore entry ID.
        user_.entry_id = entry_id;

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
    if (!ui->checkbox_disable->isChecked())
        flags |= User::ENABLED;

    user_.sessions = sessions;
    user_.flags = flags;

    LOG(INFO) << "[ACTION] Action accepted";
    accept();
    close();
}

//--------------------------------------------------------------------------------------------------
void RouterUserDialog::setAccountChanged(bool changed)
{
    account_changed_ = changed;

    ui->edit_password->setEnabled(changed);
    ui->edit_password_retry->setEnabled(changed);

    ui->edit_password->clear();
    ui->edit_password_retry->clear();

    ui->edit_password->setShowPassword(!changed);
    ui->edit_password_retry->setShowPassword(!changed);

    if (changed)
    {
        ui->edit_password->setPlaceholderText(QString());
        ui->edit_password_retry->setPlaceholderText(QString());
    }
    else
    {
        QString prompt = tr("Double-click to change");
        ui->edit_password->setPlaceholderText(prompt);
        ui->edit_password_retry->setPlaceholderText(prompt);

        ui->edit_password->installEventFilter(this);
        ui->edit_password_retry->installEventFilter(this);
    }
}

//--------------------------------------------------------------------------------------------------
// static
QString RouterUserDialog::sessionTypeToString(proto::router::SessionType session_type)
{
    const char* str = nullptr;

    switch (session_type)
    {
        case proto::router::SESSION_TYPE_ADMIN:
            str = QT_TR_NOOP("Administrator");
            break;

        case proto::router::SESSION_TYPE_MANAGER:
            str = QT_TR_NOOP("Manager");
            break;

        case proto::router::SESSION_TYPE_CLIENT:
            str = QT_TR_NOOP("Client");
            break;

        default:
            break;
    }

    if (!str)
        return QString();

    return tr(str);
}
