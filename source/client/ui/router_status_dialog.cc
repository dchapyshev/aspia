//
// Aspia Project
// Copyright (C) 2016-2022 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "client/ui/router_status_dialog.h"

#include <QAbstractButton>
#include <QPushButton>

namespace client {

RouterStatusDialog::RouterStatusDialog(QWidget* parent)
    : QDialog(parent)
{
    ui.setupUi(this);

    QPushButton* cancel_button = ui.buttonbox->button(QDialogButtonBox::StandardButton::Cancel);
    if (cancel_button)
        cancel_button->setText(tr("Cancel"));

    connect(ui.buttonbox, &QDialogButtonBox::clicked, this, [this](QAbstractButton* button)
    {
        QDialogButtonBox::StandardButton standard_button = ui.buttonbox->standardButton(button);
        if (standard_button != QDialogButtonBox::Cancel)
            return;

        emit canceled();
        close();
    });
}

RouterStatusDialog::~RouterStatusDialog() = default;

void RouterStatusDialog::setStatus(const QString& message)
{
    ui.label->setText(message);
}

} // namespace client
