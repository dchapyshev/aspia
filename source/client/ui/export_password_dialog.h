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

#ifndef CLIENT_UI_EXPORT_PASSWORD_DIALOG_H
#define CLIENT_UI_EXPORT_PASSWORD_DIALOG_H

#include <QDialog>

#include "ui_export_password_dialog.h"

class ExportPasswordDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit ExportPasswordDialog(QWidget* parent = nullptr);
    ~ExportPasswordDialog() final;

    QString password() const;

private slots:
    void onEncryptClicked();

private:
    Ui::ExportPasswordDialog ui;

    Q_DISABLE_COPY_MOVE(ExportPasswordDialog)
};

#endif // CLIENT_UI_EXPORT_PASSWORD_DIALOG_H
