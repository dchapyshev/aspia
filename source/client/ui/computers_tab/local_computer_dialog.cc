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

#include "client/ui/computers_tab/local_computer_dialog.h"

#include "base/logging.h"

#include <QAbstractButton>
#include <QMessageBox>
#include <QPushButton>

namespace client {

//--------------------------------------------------------------------------------------------------
LocalComputerDialog::LocalComputerDialog(Mode mode, QWidget* parent)
    : QDialog(parent),
      mode_(mode)
{
    LOG(INFO) << "Ctor";

    ui.setupUi(this);

    if (mode == Mode::ADD)
        setWindowTitle(tr("Add Computer"));
    else
        setWindowTitle(tr("Edit Computer"));

    connect(ui.button_show_password, &QToolButton::toggled,
            this, &LocalComputerDialog::onShowPasswordButtonToggled);

    connect(ui.button_box, &QDialogButtonBox::clicked,
            this, &LocalComputerDialog::onButtonBoxClicked);

    ui.edit_name->setFocus();
}

//--------------------------------------------------------------------------------------------------
LocalComputerDialog::~LocalComputerDialog()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void LocalComputerDialog::setComputer(const ComputerData& computer)
{
    computer_id_ = computer.id;
    group_id_ = computer.group_id;

    ui.edit_name->setText(computer.name);
    ui.edit_address->setText(computer.address);
    ui.edit_username->setText(computer.username);
    ui.edit_password->setText(computer.password);
    ui.edit_comment->setPlainText(computer.comment);
}

//--------------------------------------------------------------------------------------------------
ComputerData LocalComputerDialog::computer() const
{
    ComputerData computer;
    computer.id = computer_id_;
    computer.group_id = group_id_;
    computer.name = ui.edit_name->text();
    computer.address = ui.edit_address->text();
    computer.username = ui.edit_username->text();
    computer.password = ui.edit_password->text();
    computer.comment = ui.edit_comment->toPlainText();
    return computer;
}

//--------------------------------------------------------------------------------------------------
void LocalComputerDialog::onShowPasswordButtonToggled(bool checked)
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
void LocalComputerDialog::onButtonBoxClicked(QAbstractButton* button)
{
    if (ui.button_box->standardButton(button) != QDialogButtonBox::Ok)
    {
        reject();
        return;
    }

    if (ui.edit_name->text().isEmpty())
    {
        QMessageBox::warning(this, tr("Warning"), tr("Name cannot be empty."));
        ui.edit_name->setFocus();
        return;
    }

    if (ui.edit_address->text().isEmpty())
    {
        QMessageBox::warning(this, tr("Warning"), tr("Address cannot be empty."));
        ui.edit_address->setFocus();
        return;
    }

    accept();
}

} // namespace client
