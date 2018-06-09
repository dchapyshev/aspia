//
// PROJECT:         Aspia
// FILE:            client/ui/authorization_dialog.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/ui/authorization_dialog.h"

namespace aspia {

AuthorizationDialog::AuthorizationDialog(QWidget* parent)
    : QDialog(parent)
{
    ui.setupUi(this);

    connect(ui.button_show_password, &QPushButton::toggled,
            this, &AuthorizationDialog::onShowPasswordButtonToggled);

    connect(ui.button_box, &QDialogButtonBox::clicked,
            this, &AuthorizationDialog::onButtonBoxClicked);
}

AuthorizationDialog::~AuthorizationDialog() = default;

QString AuthorizationDialog::userName() const
{
    return ui.edit_username->text();
}

void AuthorizationDialog::setUserName(const QString& username)
{
    ui.edit_username->setText(username);
}

QString AuthorizationDialog::password() const
{
    return ui.edit_password->text();
}

void AuthorizationDialog::setPassword(const QString& password)
{
    ui.edit_password->setText(password);
}

void AuthorizationDialog::showEvent(QShowEvent* event)
{
    if (ui.edit_username->text().isEmpty())
        ui.edit_username->setFocus();
    else
        ui.edit_password->setFocus();

    QDialog::showEvent(event);
}

void AuthorizationDialog::onShowPasswordButtonToggled(bool checked)
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

void AuthorizationDialog::onButtonBoxClicked(QAbstractButton* button)
{
    if (ui.button_box->standardButton(button) == QDialogButtonBox::Ok)
        accept();
    else
        reject();

    close();
}

} // namespace aspia
