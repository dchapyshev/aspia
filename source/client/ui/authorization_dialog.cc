//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "client/ui/authorization_dialog.h"

#include "qt_base/qt_logging.h"

#include <QMessageBox>
#include <QTimer>

namespace client {

AuthorizationDialog::AuthorizationDialog(QWidget* parent)
    : QDialog(parent)
{
    LOG(LS_INFO) << "Ctor";
    ui.setupUi(this);

    connect(ui.button_show_password, &QPushButton::toggled,
            this, &AuthorizationDialog::onShowPasswordButtonToggled);

    connect(ui.buttonbox, &QDialogButtonBox::clicked,
            this, &AuthorizationDialog::onButtonBoxClicked);

    connect(ui.checkbox_one_time_password, &QCheckBox::toggled, this, [this](bool checked)
    {
        ui.label_username->setVisible(!checked);
        ui.edit_username->setVisible(!checked);
        ui.edit_username->clear();

        fitSize();
    });

    fitSize();
}

AuthorizationDialog::~AuthorizationDialog()
{
    LOG(LS_INFO) << "Dtor";
}

void AuthorizationDialog::setOneTimePasswordEnabled(bool enable)
{
    ui.checkbox_one_time_password->setVisible(enable);
    fitSize();
}

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
    if (ui.edit_username->text().isEmpty() && !ui.checkbox_one_time_password->isChecked())
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

    ui.edit_password->setFocus();
}

void AuthorizationDialog::onButtonBoxClicked(QAbstractButton* button)
{
    if (ui.buttonbox->standardButton(button) == QDialogButtonBox::Ok)
    {
        if (!ui.checkbox_one_time_password->isChecked())
        {
            if (ui.edit_username->text().isEmpty())
            {
                QMessageBox::warning(this,
                                     tr("Warning"),
                                     tr("Username cannot be empty."),
                                     QMessageBox::Ok);
                return;
            }
        }

        if (ui.edit_password->text().isEmpty())
        {
            QMessageBox::warning(this,
                                 tr("Warning"),
                                 tr("Password cannot be empty."),
                                 QMessageBox::Ok);
            return;
        }

        accept();
    }
    else
    {
        reject();
    }

    close();
}

void AuthorizationDialog::fitSize()
{
    QTimer::singleShot(0, this, [this]()
    {
        setFixedHeight(sizeHint().height());
    });
}

} // namespace client
