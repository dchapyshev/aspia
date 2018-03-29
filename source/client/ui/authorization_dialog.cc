//
// PROJECT:         Aspia
// FILE:            client/ui/authorization_dialog.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/ui/authorization_dialog.h"

namespace aspia {

AuthorizationDialog::AuthorizationDialog(proto::Computer* computer, QWidget* parent)
    : QDialog(parent),
      computer_(computer)
{
    ui.setupUi(this);

    ui.edit_username->setText(QString::fromUtf8(computer_->username().c_str(),
                                                computer_->username().size()));
    ui.edit_password->setText(QString::fromUtf8(computer_->password().c_str(),
                                                computer_->password().size()));

    connect(ui.button_show_password, &QPushButton::toggled,
            this, &AuthorizationDialog::OnShowPasswordButtonToggled);

    connect(ui.button_box, &QDialogButtonBox::clicked,
            this, &AuthorizationDialog::OnButtonBoxClicked);
}

void AuthorizationDialog::OnShowPasswordButtonToggled(bool checked)
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

void AuthorizationDialog::OnButtonBoxClicked(QAbstractButton* button)
{
    if (ui.button_box->standardButton(button) == QDialogButtonBox::Ok)
    {
        computer_->set_username(ui.edit_username->text().toUtf8());
        computer_->set_password(ui.edit_password->text().toUtf8());
        accept();
    }
    else
    {
        reject();
    }

    close();
}

} // namespace aspia
