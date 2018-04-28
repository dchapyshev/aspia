//
// PROJECT:         Aspia
// FILE:            console/open_address_book_dialog.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "console/open_address_book_dialog.h"

namespace aspia {

OpenAddressBookDialog::OpenAddressBookDialog(
    QWidget* parent, proto::address_book::EncryptionType encryption_type)
    : QDialog(parent)
{
    ui.setupUi(this);

    connect(ui.button_show_password, &QPushButton::toggled,
            this, &OpenAddressBookDialog::showPasswordButtonToggled);

    connect(ui.button_box, &QDialogButtonBox::clicked,
            this, &OpenAddressBookDialog::buttonBoxClicked);

    switch (encryption_type)
    {
        case proto::address_book::ENCRYPTION_TYPE_NONE:
            ui.edit_encryption_type->setText(tr("Without Encryption"));
            break;

        case proto::address_book::ENCRYPTION_TYPE_XCHACHA20_POLY1305:
            ui.edit_encryption_type->setText(tr("XChaCha20 + Poly1305 (256-bit key)"));
            break;

        default:
            qFatal("Unknown encryption type: %d", encryption_type);
            break;
    }

    ui.edit_password->setFocus();
}

OpenAddressBookDialog::~OpenAddressBookDialog() = default;

QString OpenAddressBookDialog::password() const
{
    return ui.edit_password->text();
}

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

void OpenAddressBookDialog::buttonBoxClicked(QAbstractButton* button)
{
    if (ui.button_box->standardButton(button) == QDialogButtonBox::Ok)
        accept();
    else
        reject();

    close();
}

} // namespace aspia
