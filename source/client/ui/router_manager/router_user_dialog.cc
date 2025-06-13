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

#include "client/ui/router_manager/router_user_dialog.h"

#include "base/logging.h"
#include "base/peer/user.h"

#include <QAbstractButton>
#include <QPushButton>
#include <QMessageBox>

namespace client {

//--------------------------------------------------------------------------------------------------
RouterUserDialog::RouterUserDialog(const base::User& user, const QStringList& users, QWidget* parent)
    : QDialog(parent),
      user_(user),
      users_(users)
{
    LOG(LS_INFO) << "Ctor";
    ui.setupUi(this);

    QPushButton* cancel_button = ui.buttonbox->button(QDialogButtonBox::StandardButton::Cancel);
    if (cancel_button)
        cancel_button->setText(tr("Cancel"));

    if (user_.isValid())
    {
        ui.checkbox_disable->setChecked(!(user_.flags & base::User::ENABLED));
        ui.edit_username->setText(user_.name);

        setAccountChanged(false);
    }
    else
    {
        ui.checkbox_disable->setChecked(false);
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

        ui.tree_sessions->addTopLevelItem(item);
    };

    add_session(proto::router::SESSION_TYPE_CLIENT);
    add_session(proto::router::SESSION_TYPE_ADMIN);

    connect(ui.buttonbox, &QDialogButtonBox::clicked, this, &RouterUserDialog::onButtonBoxClicked);
    connect(ui.edit_username, &QLineEdit::textEdited, this, [this]()
    {
        setAccountChanged(true);
    });
}

//--------------------------------------------------------------------------------------------------
RouterUserDialog::~RouterUserDialog()
{
    LOG(LS_INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
const base::User& RouterUserDialog::user() const
{
    return user_;
}

//--------------------------------------------------------------------------------------------------
bool RouterUserDialog::eventFilter(QObject* object, QEvent* event)
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

//--------------------------------------------------------------------------------------------------
void RouterUserDialog::onButtonBoxClicked(QAbstractButton* button)
{
    QDialogButtonBox::StandardButton standard_button = ui.buttonbox->standardButton(button);
    if (standard_button != QDialogButtonBox::Ok)
    {
        LOG(LS_INFO) << "[ACTION] Action rejected";
        reject();
        close();
        return;
    }

    if (account_changed_)
    {
        QString username = ui.edit_username->text();

        if (!base::User::isValidUserName(username))
        {
            LOG(LS_ERROR) << "Invalid user name:" << username;
            QMessageBox::warning(this,
                                 tr("Warning"),
                                 tr("The user name can not be empty and can contain only alphabet"
                                    " characters, numbers and ""_"", ""-"", ""."", ""@"" characters."),
                                 QMessageBox::Ok);

            ui.edit_username->selectAll();
            ui.edit_username->setFocus();
            return;
        }

        for (QStringList::size_type i = 0; i < users_.size(); ++i)
        {
            if (username.compare(users_.at(i), Qt::CaseInsensitive) == 0)
            {
                LOG(LS_ERROR) << "User name already exists:" << username;
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
            LOG(LS_INFO) << "Passwords do not match";
            QMessageBox::warning(this,
                                 tr("Warning"),
                                 tr("The passwords you entered do not match."),
                                 QMessageBox::Ok);

            ui.edit_password->selectAll();
            ui.edit_password->setFocus();
            return;
        }

        QString password = ui.edit_password->text();

        if (!base::User::isValidPassword(password))
        {
            LOG(LS_INFO) << "Invalid password";
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
                ui.edit_password_retry->clear();
                ui.edit_password->setFocus();
                return;
            }
        }

        // Save entry ID.
        qint64 entry_id = user_.entry_id;

        // Create new user.
        user_ = base::User::create(username, password);

        // Restore entry ID.
        user_.entry_id = entry_id;

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
    if (!ui.checkbox_disable->isChecked())
        flags |= base::User::ENABLED;

    user_.sessions = sessions;
    user_.flags = flags;

    LOG(LS_INFO) << "[ACTION] Action accepted";
    accept();
    close();
}

//--------------------------------------------------------------------------------------------------
void RouterUserDialog::setAccountChanged(bool changed)
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

} // namespace client
