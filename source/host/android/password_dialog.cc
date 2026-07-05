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

#include "host/android/password_dialog.h"

#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

#include "common/android/button.h"
#include "common/android/controls.h"
#include "common/android/dialog.h"
#include "common/android/line_edit.h"
#include "host/database.h"

namespace {

//--------------------------------------------------------------------------------------------------
// Inline error label placed below the fields; the hex color follows the theme's error color.
QLabel* createErrorLabel()
{
    QLabel* error = new QLabel();
    error->setWordWrap(true);
    error->setStyleSheet(QString("color: %1").arg(Controls::errorColor().name()));
    error->hide();
    return error;
}

} // namespace

//--------------------------------------------------------------------------------------------------
// static
bool PasswordDialog::verify(QWidget* parent)
{
    Dialog dialog(parent);
    dialog.setTitle(tr("Enter Password"));

    LineEdit* edit = new LineEdit();
    edit->setLabel(tr("Password"));
    edit->setEchoMode(QLineEdit::Password);

    QLabel* error = createErrorLabel();

    dialog.contentLayout()->addWidget(edit);
    dialog.contentLayout()->addWidget(error);

    Button* cancel = dialog.addButton(tr("Cancel"), Button::Role::TEXT);
    Button* ok = dialog.addButton(tr("OK"), Button::Role::FILLED);

    QObject::connect(cancel, &Button::clicked, &dialog, &QDialog::reject);
    QObject::connect(ok, &Button::clicked, &dialog, [&dialog, edit, error]()
    {
        if (Database::instance().verifyPassword(SecureString(edit->text())))
        {
            dialog.accept();
        }
        else
        {
            error->setText(tr("You entered an incorrect password."));
            error->show();
            edit->selectAll();
            edit->setFocus();
        }
    });

    return dialog.exec() == QDialog::Accepted;
}

//--------------------------------------------------------------------------------------------------
// static
std::optional<SecureString> PasswordDialog::create(QWidget* parent)
{
    Dialog dialog(parent);
    dialog.setTitle(tr("Set Password"));

    LineEdit* password = new LineEdit();
    password->setLabel(tr("New password"));
    password->setEchoMode(QLineEdit::Password);

    LineEdit* repeat = new LineEdit();
    repeat->setLabel(tr("Repeat password"));
    repeat->setEchoMode(QLineEdit::Password);

    QLabel* error = createErrorLabel();

    dialog.contentLayout()->addWidget(password);
    dialog.contentLayout()->addWidget(repeat);
    dialog.contentLayout()->addWidget(error);

    Button* cancel = dialog.addButton(tr("Cancel"), Button::Role::TEXT);
    Button* ok = dialog.addButton(tr("OK"), Button::Role::FILLED);

    std::optional<SecureString> result;

    QObject::connect(cancel, &Button::clicked, &dialog, &QDialog::reject);
    QObject::connect(ok, &Button::clicked, &dialog, [&]()
    {
        if (password->text().isEmpty())
        {
            error->setText(tr("The password can not be empty."));
            error->show();
            password->setFocus();
            return;
        }

        if (password->text() != repeat->text())
        {
            error->setText(tr("The passwords you entered do not match."));
            error->show();
            repeat->selectAll();
            repeat->setFocus();
            return;
        }

        result = SecureString(password->text());
        dialog.accept();
    });

    dialog.exec();
    return result;
}
