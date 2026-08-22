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
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QTimer>

#include "base/gui_application.h"
#include "base/crypto/totp.h"
#include "ui_two_factor_code_dialog.h"

//--------------------------------------------------------------------------------------------------
TwoFactorCodeDialog::TwoFactorCodeDialog(bool code_refused, QWidget* parent)
    : QDialog(parent),
      ui(std::make_unique<Ui::TwoFactorCodeDialog>())
{
    ui->setupUi(this);

    if (code_refused)
    {
        ui->label_prompt->setText(tr("The previous code was not accepted.") + ' ' +
                                  ui->label_prompt->text());
    }

    ui->label_icon->setPixmap(GuiApplication::svgPixmap(":/img/lock.svg", QSize(48, 48)));
    ui->edit_code->setValidator(
        new QRegularExpressionValidator(QRegularExpression("\\d*"), ui->edit_code));
    ui->edit_code->setFocus();

    // The router refuses a code of the wrong length the way it refuses a wrong one: it ends the
    // session and counts the attempt against the block. So an incomplete code never leaves here.
    QPushButton* ok_button = ui->buttonbox->button(QDialogButtonBox::Ok);
    ok_button->setEnabled(false);

    connect(ui->edit_code, &QLineEdit::textChanged, this, [ok_button](const QString& text)
    {
        ok_button->setEnabled(text.trimmed().size() == Totp::kDefaultDigits);
    });

    connect(ui->buttonbox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(ui->buttonbox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    // Pin the height to the layout's preferred size so the user cannot drag the dialog into
    // an awkward tall shape. Defer to the next event loop tick so the layout has settled.
    QTimer::singleShot(MilliSeconds(0), this, [this]() { setFixedHeight(sizeHint().height()); });
}

//--------------------------------------------------------------------------------------------------
TwoFactorCodeDialog::~TwoFactorCodeDialog() = default;

//--------------------------------------------------------------------------------------------------
QString TwoFactorCodeDialog::code() const
{
    return ui->edit_code->text().trimmed();
}
