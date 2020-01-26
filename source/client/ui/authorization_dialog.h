//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef CLIENT__UI__AUTHORIZATION_DIALOG_H
#define CLIENT__UI__AUTHORIZATION_DIALOG_H

#include "base/macros_magic.h"
#include "ui_authorization_dialog.h"

class QAbstractButton;

namespace client {

class AuthorizationDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AuthorizationDialog(QWidget* parent = nullptr);
    ~AuthorizationDialog();

    QString userName() const;
    void setUserName(const QString& username);

    QString password() const;
    void setPassword(const QString& password);

protected:
    // QDialog implementation.
    void showEvent(QShowEvent* event) override;

private slots:
    void onShowPasswordButtonToggled(bool checked);
    void onButtonBoxClicked(QAbstractButton* button);

private:
    Ui::AuthorizationDialog ui;

    DISALLOW_COPY_AND_ASSIGN(AuthorizationDialog);
};

} // namespace client

#endif // CLIENT__UI__AUTHORIZATION_DIALOG_H
