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

#include "host/ui/check_password_dialog.h"

#include <QAbstractButton>
#include <QPushButton>
#include <QTimer>

#include "base/logging.h"
#include "base/crypto/secure_string.h"
#include "common/ui/msg_box.h"
#include "common/ui/password_edit.h"
#include "host/system_settings.h"
#include "ui_check_password_dialog.h"

//--------------------------------------------------------------------------------------------------
CheckPasswordDialog::CheckPasswordDialog(QWidget* parent)
    : QDialog(parent),
      ui(std::make_unique<Ui::CheckPasswordDialog>())
{
    LOG(INFO) << "Ctor";
    ui->setupUi(this);

    connect(ui->button_box, &QDialogButtonBox::clicked,
            this, &CheckPasswordDialog::onButtonBoxClicked);

    QTimer::singleShot(0, this, [this]()
    {
        setFixedHeight(sizeHint().height());
    });
}

//--------------------------------------------------------------------------------------------------
CheckPasswordDialog::~CheckPasswordDialog()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void CheckPasswordDialog::onButtonBoxClicked(QAbstractButton* button)
{
    QDialogButtonBox::StandardButton standard_button = ui->button_box->standardButton(button);
    if (standard_button == QDialogButtonBox::Ok)
    {
        LOG(INFO) << "[ACTION] Accepted by user";

        SecureString password = ui->edit_pass->password();

        if (!SystemSettings::isValidPassword(password))
        {
            LOG(INFO) << "Invalid password entered";

            MsgBox::warning(this, tr("You entered an incorrect password."));
            ui->edit_pass->selectAll();
            ui->edit_pass->setFocus();
            return;
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
