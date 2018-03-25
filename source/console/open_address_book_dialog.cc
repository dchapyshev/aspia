//
// PROJECT:         Aspia
// FILE:            console/open_address_book_dialog.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "console/open_address_book_dialog.h"

#include "base/logging.h"

namespace aspia {

OpenAddressBookDialog::OpenAddressBookDialog(
    QWidget* parent, proto::AddressBook::EncryptionType encryption_type)
    : QDialog(parent)
{
    ui.setupUi(this);

    connect(ui.button_show_password,
            SIGNAL(toggled(bool)),
            this,
            SLOT(OnShowPasswordButtonToggled(bool)));

    connect(ui.button_box,
            SIGNAL(clicked(QAbstractButton*)),
            this,
            SLOT(OnButtonBoxClicked(QAbstractButton*)));

    switch (encryption_type)
    {
        case proto::AddressBook::ENCRYPTION_TYPE_NONE:
            ui.edit_encryption_type->setText(tr("Without Encryption"));
            break;

        case proto::AddressBook::ENCRYPTION_TYPE_XCHACHA20_POLY1305:
            ui.edit_encryption_type->setText(tr("XChaCha20 + Poly1305 (256-bit key)"));
            break;

        default:
            DLOG(LS_FATAL) << "Unknown encryption type: " << encryption_type;
            break;
    }

    ui.edit_password->setFocus();
}

QString OpenAddressBookDialog::Password() const
{
    return ui.edit_password->text();
}

void OpenAddressBookDialog::OnShowPasswordButtonToggled(bool checked)
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

void OpenAddressBookDialog::OnButtonBoxClicked(QAbstractButton* button)
{
    if (ui.button_box->standardButton(button) == QDialogButtonBox::Ok)
    {
        accept();
    }
    else
    {
        reject();
    }

    close();
}

} // namespace aspia
