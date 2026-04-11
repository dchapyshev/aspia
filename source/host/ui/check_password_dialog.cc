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

#include "base/gui_application.h"
#include "common/ui/msg_box.h"
#include "base/logging.h"
#include "host/system_settings.h"

namespace host {

//--------------------------------------------------------------------------------------------------
CheckPasswordDialog::CheckPasswordDialog(QWidget* parent)
    : QDialog(parent)
{
    LOG(INFO) << "Ctor";
    ui.setupUi(this);

    base::GuiApplication::translateButtonBox(ui.button_box);

    connect(ui.button_box, &QDialogButtonBox::clicked,
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
    QDialogButtonBox::StandardButton standard_button = ui.button_box->standardButton(button);
    if (standard_button == QDialogButtonBox::Ok)
    {
        LOG(INFO) << "[ACTION] Accepted by user";

        QString password = ui.edit_pass->text();

        if (!SystemSettings::isValidPassword(password))
        {
            LOG(INFO) << "Invalid password entered";

            common::MsgBox::warning(this, tr("You entered an incorrect password."));
            ui.edit_pass->selectAll();
            ui.edit_pass->setFocus();
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

} // namespace host
