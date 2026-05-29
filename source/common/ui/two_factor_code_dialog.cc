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

#include "common/ui/two_factor_code_dialog.h"

#include "ui_two_factor_code_dialog.h"

//--------------------------------------------------------------------------------------------------
TwoFactorCodeDialog::TwoFactorCodeDialog(QWidget* parent)
    : QDialog(parent),
      ui(std::make_unique<Ui::TwoFactorCodeDialog>())
{
    ui->setupUi(this);
    ui->edit_code->setFocus();
}

//--------------------------------------------------------------------------------------------------
TwoFactorCodeDialog::~TwoFactorCodeDialog() = default;

//--------------------------------------------------------------------------------------------------
QString TwoFactorCodeDialog::code() const
{
    return ui->edit_code->text().trimmed();
}
