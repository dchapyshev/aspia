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

#include "common/android/message_dialog.h"

#include <QDialog>

#include "common/android/dialog.h"

//--------------------------------------------------------------------------------------------------
// static
bool MessageDialog::confirm(QWidget* parent, const QString& title, const QString& text,
                            const QString& accept_text)
{
    Dialog dialog(parent);
    dialog.setTitle(title);
    dialog.setText(text);

    Button* cancel = dialog.addButton(QObject::tr("Cancel"), Button::Role::TEXT);
    Button* accept = dialog.addButton(accept_text, Button::Role::FILLED);

    QObject::connect(cancel, &Button::clicked, &dialog, &QDialog::reject);
    QObject::connect(accept, &Button::clicked, &dialog, &QDialog::accept);

    return dialog.exec() == QDialog::Accepted;
}

//--------------------------------------------------------------------------------------------------
// static
void MessageDialog::info(QWidget* parent, const QString& title, const QString& text)
{
    Dialog dialog(parent);
    dialog.setTitle(title);
    dialog.setText(text);

    Button* ok = dialog.addButton(QObject::tr("OK"), Button::Role::FILLED);
    QObject::connect(ok, &Button::clicked, &dialog, &QDialog::accept);

    dialog.exec();
}
