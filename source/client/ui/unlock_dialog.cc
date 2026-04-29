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

#include "client/ui/unlock_dialog.h"

#include <QAbstractButton>
#include <QPushButton>
#include <QTimer>

#include "base/logging.h"

namespace client {

//--------------------------------------------------------------------------------------------------
UnlockDialog::UnlockDialog(QWidget* parent,
                           const QString& file_path,
                           const QString& encryption_type)
    : QDialog(parent)
{
    LOG(INFO) << "Ctor";
    ui.setupUi(this);

    connect(ui.button_show_password, &QPushButton::toggled, this, &UnlockDialog::onShowPasswordButtonToggled);
    connect(ui.button_box, &QDialogButtonBox::clicked, this, &UnlockDialog::onButtonBoxClicked);

    if (file_path.isEmpty())
    {
        ui.label_file->setVisible(false);
        ui.edit_file->setVisible(false);
    }
    else
    {
        ui.edit_file->setText(file_path);
    }

    if (encryption_type.isEmpty())
    {
        ui.label_encryption_type->setVisible(false);
        ui.edit_encryption_type->setVisible(false);
    }
    else
    {
        ui.edit_encryption_type->setText(encryption_type);
    }

    ui.edit_password->setFocus();

    QTimer::singleShot(0, this, [this](){ setFixedHeight(sizeHint().height()); });
}

//--------------------------------------------------------------------------------------------------
UnlockDialog::~UnlockDialog()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
QString UnlockDialog::password() const
{
    return ui.edit_password->text();
}

//--------------------------------------------------------------------------------------------------
void UnlockDialog::onShowPasswordButtonToggled(bool checked)
{
    if (checked)
    {
        ui.edit_password->setEchoMode(QLineEdit::Normal);
        ui.edit_password->setInputMethodHints(Qt::ImhNone);
    }
    else
    {
        ui.edit_password->setEchoMode(QLineEdit::Password);
        ui.edit_password->setInputMethodHints(Qt::ImhHiddenText | Qt::ImhSensitiveData |
                                              Qt::ImhNoAutoUppercase | Qt::ImhNoPredictiveText);
    }
}

//--------------------------------------------------------------------------------------------------
void UnlockDialog::onButtonBoxClicked(QAbstractButton* button)
{
    if (ui.button_box->standardButton(button) == QDialogButtonBox::Ok)
        accept();
    else
        reject();

    close();
}

} // namespace client
