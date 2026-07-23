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

#include "common/desktop/two_factor_code_dialog.h"

#include <QDialogButtonBox>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QTimer>

#include "base/gui_application.h"
#include "ui_two_factor_code_dialog.h"

//--------------------------------------------------------------------------------------------------
TwoFactorCodeDialog::TwoFactorCodeDialog(QWidget* parent)
    : QDialog(parent),
      ui(std::make_unique<Ui::TwoFactorCodeDialog>())
{
    ui->setupUi(this);
    ui->label_icon->setPixmap(GuiApplication::svgPixmap(":/img/lock.svg", QSize(48, 48)));
    ui->edit_code->setValidator(
        new QRegularExpressionValidator(QRegularExpression("\\d*"), ui->edit_code));
    ui->edit_code->setFocus();

    connect(ui->buttonbox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(ui->buttonbox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    // Pin the height to the layout's preferred size so the user cannot drag the dialog into
    // an awkward tall shape. Defer to the next event loop tick so the layout has settled.
    QTimer::singleShot(Milliseconds(0), this, [this]() { setFixedHeight(sizeHint().height()); });
}

//--------------------------------------------------------------------------------------------------
TwoFactorCodeDialog::~TwoFactorCodeDialog() = default;

//--------------------------------------------------------------------------------------------------
QString TwoFactorCodeDialog::code() const
{
    return ui->edit_code->text().trimmed();
}
