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

#include "client/ui/hosts/router_host_dialog.h"

#include <QAbstractButton>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QToolButton>

#include "base/crypto/secure_string.h"
#include "base/logging.h"
#include "common/ui/msg_box.h"
#include "common/ui/password_edit.h"
#include "proto/router_constants.h"
#include "proto/router_manager.h"
#include "ui_router_host_dialog.h"

//--------------------------------------------------------------------------------------------------
RouterHostDialog::RouterHostDialog(qint64 router_id, const Router::Host& host, QWidget* parent)
    : QDialog(parent),
      ui(std::make_unique<Ui::RouterHostDialog>()),
      router_id_(router_id),
      host_(host)
{
    LOG(INFO) << "Ctor";
    ui->setupUi(this);

    ui->edit_display_name->setText(host_.display_name);
    ui->edit_user_name->setText(host_.user_name);
    ui->edit_password->setPassword(SecureString(host_.password));
    ui->edit_comment->setPlainText(host_.comment);

    connect(ui->button_show_password, &QToolButton::toggled, ui->edit_password, &PasswordEdit::setShowPassword);
    connect(ui->buttonbox, &QDialogButtonBox::clicked, this, &RouterHostDialog::onButtonBoxClicked);
}

//--------------------------------------------------------------------------------------------------
RouterHostDialog::~RouterHostDialog()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void RouterHostDialog::onHostEditResultReceived(const proto::router::HostEditResult& result)
{
    const std::string& error_code = result.error_code();
    if (error_code != proto::router::kErrorOk)
    {
        const char* message;

        if (error_code == proto::router::kErrorAccessDenied)
            message = QT_TR_NOOP("Access denied.");
        else if (error_code == proto::router::kErrorNotFound)
            message = QT_TR_NOOP("Host not found.");
        else if (error_code == proto::router::kErrorInternalError)
            message = QT_TR_NOOP("Unknown internal error.");
        else
            message = QT_TR_NOOP("Unknown error type.");

        MsgBox::warning(this, tr(message));
        ui->buttonbox->button(QDialogButtonBox::Ok)->setEnabled(true);
        return;
    }

    accept();
}

//--------------------------------------------------------------------------------------------------
void RouterHostDialog::onButtonBoxClicked(QAbstractButton* button)
{
    QDialogButtonBox::StandardButton standard_button = ui->buttonbox->standardButton(button);
    if (standard_button != QDialogButtonBox::Ok)
    {
        LOG(INFO) << "[ACTION] Edit host rejected";
        reject();
        return;
    }

    Router* router = Router::instance(router_id_);
    if (!router)
    {
        LOG(ERROR) << "Router not available for id:" << router_id_;
        reject();
        return;
    }

    host_.display_name = ui->edit_display_name->text();
    host_.user_name    = ui->edit_user_name->text();
    host_.password     = ui->edit_password->password().toString();
    host_.comment      = ui->edit_comment->toPlainText();

    LOG(INFO) << "[ACTION] Edit host accepted, sending request";
    ui->buttonbox->button(QDialogButtonBox::Ok)->setEnabled(false);
    router->editHost(host_, this, &RouterHostDialog::onHostEditResultReceived);
}
