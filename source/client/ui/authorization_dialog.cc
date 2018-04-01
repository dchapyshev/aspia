//
// PROJECT:         Aspia
// FILE:            client/ui/authorization_dialog.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/ui/authorization_dialog.h"

#include "protocol/computer.pb.h"

namespace aspia {

AuthorizationDialog::AuthorizationDialog(proto::Computer* computer, QWidget* parent)
    : QDialog(parent),
      computer_(computer)
{
    ui.setupUi(this);

    ui.edit_username->setText(QString::fromStdString(computer_->username()));
    ui.edit_password->setText(QString::fromStdString(computer_->password()));

    connect(ui.button_show_password, &QPushButton::toggled,
            this, &AuthorizationDialog::OnShowPasswordButtonToggled);

    connect(ui.button_box, &QDialogButtonBox::clicked,
            this, &AuthorizationDialog::OnButtonBoxClicked);
}

AuthorizationDialog::~AuthorizationDialog() = default;

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
        computer_->set_username(ui.edit_username->text().toStdString());
        computer_->set_password(ui.edit_password->text().toStdString());
        accept();
    }
    else
    {
        reject();
    }

    close();
}

} // namespace aspia
