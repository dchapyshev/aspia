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

#include "client/ui/hosts/router_password_dialog.h"

#include <QAbstractButton>

#include "base/logging.h"
#include "base/crypto/secure_string.h"
#include "base/peer/user.h"
#include "client/router.h"
#include "common/ui/msg_box.h"
#include "common/ui/password_edit.h"
#include "proto/router_constants.h"
#include "ui_router_password_dialog.h"

//--------------------------------------------------------------------------------------------------
RouterPasswordDialog::RouterPasswordDialog(qint64 router_id, QWidget* parent)
    : QDialog(parent),
      ui(std::make_unique<Ui::RouterPasswordDialog>()),
      router_id_(router_id)
{
    LOG(INFO) << "Ctor";
    ui->setupUi(this);

    connect(ui->buttonbox, &QDialogButtonBox::clicked, this, &RouterPasswordDialog::onButtonBoxClicked);

    ui->edit_password->setFocus();
}

//--------------------------------------------------------------------------------------------------
RouterPasswordDialog::~RouterPasswordDialog()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void RouterPasswordDialog::onChangePasswordResult(const proto::router::ChangePasswordResult& result)
{
    const std::string& error_code = result.error_code();
    if (error_code == proto::router::kErrorOk)
    {
        LOG(INFO) << "[ACTION] Password changed";
        accept();
        close();
        return;
    }

    const char* message;
    if (error_code == proto::router::kErrorInvalidRequest)
        message = QT_TR_NOOP("Invalid password change request.");
    else if (error_code == proto::router::kErrorInternalError)
        message = QT_TR_NOOP("Unknown internal error.");
    else if (error_code == proto::router::kErrorInvalidData)
        message = QT_TR_NOOP("Invalid data was passed.");
    else
        message = QT_TR_NOOP("Unknown error type.");

    LOG(ERROR) << "Password change failed:" << error_code;
    setEnabled(true);
    MsgBox::warning(this, tr(message));
}

//--------------------------------------------------------------------------------------------------
void RouterPasswordDialog::onButtonBoxClicked(QAbstractButton* button)
{
    QDialogButtonBox::StandardButton standard_button = ui->buttonbox->standardButton(button);
    if (standard_button != QDialogButtonBox::Ok)
    {
        LOG(INFO) << "[ACTION] Action rejected";
        reject();
        close();
        return;
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

        MsgBox message_box(MsgBox::Warning, tr("Warning"),
            QString("<b>%1</b><br/>%2<br/>%3").arg(unsafe, safe, question),
            MsgBox::Yes | MsgBox::No, this);
        if (message_box.exec() == MsgBox::Yes)
        {
            ui->edit_password->clear();
            ui->edit_password_retry->clear();
            ui->edit_password->setFocus();
            return;
        }
    }

    Router* router = Router::instance(router_id_);
    if (!router)
    {
        LOG(ERROR) << "Router instance is gone";
        return;
    }

    setEnabled(false);

    LOG(INFO) << "[ACTION] Submitting password change";
    router->changePassword(password, this, &RouterPasswordDialog::onChangePasswordResult);
}
