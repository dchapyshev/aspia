//
// Aspia Project
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

#ifndef CLIENT_UI_AUTHORIZATION_DIALOG_H
#define CLIENT_UI_AUTHORIZATION_DIALOG_H

#include "ui_authorization_dialog.h"

class QAbstractButton;

namespace client {

class AuthorizationDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit AuthorizationDialog(QWidget* parent = nullptr);
    ~AuthorizationDialog() final;

    void setOneTimePasswordEnabled(bool enable);

    QString userName() const;
    void setUserName(const QString& username);

    QString password() const;
    void setPassword(const QString& password);

protected:
    // QDialog implementation.
    void showEvent(QShowEvent* event) final;

private slots:
    void onShowPasswordButtonToggled(bool checked);
    void onOneTimePasswordToggled(bool checked);
    void onButtonBoxClicked(QAbstractButton* button);

private:
    void fitSize();

    Ui::AuthorizationDialog ui;
    Q_DISABLE_COPY(AuthorizationDialog)
};

} // namespace client

#endif // CLIENT_UI_AUTHORIZATION_DIALOG_H
