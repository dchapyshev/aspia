//
// SmartCafe Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef CLIENT_UI_CLIENT_SETTINGS_DIALOG_H
#define CLIENT_UI_CLIENT_SETTINGS_DIALOG_H

#include "ui_client_settings_dialog.h"

#include <QDialog>

class QAbstractButton;

namespace client {

class ClientSettingsDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit ClientSettingsDialog(QWidget* parent = nullptr);
    ~ClientSettingsDialog() final;

protected:
    void closeEvent(QCloseEvent* event) final;

private slots:
    void onButtonBoxClicked(QAbstractButton* button);

private:
    void showError(const QString& message);

    Ui::ClientSettingsDialog ui;
};

} // namespace client

#endif // CLIENT_UI_CLIENT_SETTINGS_DIALOG_H
