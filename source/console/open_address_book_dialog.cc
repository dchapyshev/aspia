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

#include "console/open_address_book_dialog.h"

#include "base/logging.h"

namespace console {

//--------------------------------------------------------------------------------------------------
OpenAddressBookDialog::OpenAddressBookDialog(QWidget* parent,
                                             const QString& file_path,
                                             proto::address_book::EncryptionType encryption_type)
    : QDialog(parent)
{
    LOG(LS_INFO) << "Ctor";
    ui.setupUi(this);

    QPushButton* cancel_button = ui.button_box->button(QDialogButtonBox::StandardButton::Cancel);
    if (cancel_button)
        cancel_button->setText(tr("Cancel"));

    setFixedHeight(sizeHint().height());

    connect(ui.button_show_password, &QPushButton::toggled,
            this, &OpenAddressBookDialog::showPasswordButtonToggled);

    connect(ui.button_box, &QDialogButtonBox::clicked,
            this, &OpenAddressBookDialog::buttonBoxClicked);

    switch (encryption_type)
    {
        case proto::address_book::ENCRYPTION_TYPE_NONE:
            ui.edit_encryption_type->setText(tr("Without Encryption"));
            break;

        case proto::address_book::ENCRYPTION_TYPE_CHACHA20_POLY1305:
            ui.edit_encryption_type->setText(tr("ChaCha20 + Poly1305 (256-bit key)"));
            break;

        default:
            LOG(LS_FATAL) << "Unknown encryption type:" << encryption_type;
            break;
    }

    ui.edit_file->setText(file_path);
    ui.edit_password->setFocus();
}

//--------------------------------------------------------------------------------------------------
OpenAddressBookDialog::~OpenAddressBookDialog()
{
    LOG(LS_INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
QString OpenAddressBookDialog::password() const
{
    return ui.edit_password->text();
}

//--------------------------------------------------------------------------------------------------
void OpenAddressBookDialog::showPasswordButtonToggled(bool checked)
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
void OpenAddressBookDialog::buttonBoxClicked(QAbstractButton* button)
{
    if (ui.button_box->standardButton(button) == QDialogButtonBox::Ok)
        accept();
    else
        reject();

    close();
}

} // namespace console
