//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef ASPIA_CLIENT__UI__AUTHORIZATION_DIALOG_H_
#define ASPIA_CLIENT__UI__AUTHORIZATION_DIALOG_H_

#include "base/macros_magic.h"
#include "ui_authorization_dialog.h"

namespace aspia {

class AuthorizationDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AuthorizationDialog(QWidget* parent = nullptr);
    ~AuthorizationDialog() = default;

    QString userName() const;
    void setUserName(const QString& username);

    QString password() const;
    void setPassword(const QString& password);

protected:
    void showEvent(QShowEvent* event) override;

private slots:
    void onShowPasswordButtonToggled(bool checked);
    void onButtonBoxClicked(QAbstractButton* button);

private:
    Ui::AuthorizationDialog ui;

    DISALLOW_COPY_AND_ASSIGN(AuthorizationDialog);
};

} // namespace aspia

#endif // ASPIA_CLIENT__UI__AUTHORIZATION_DIALOG_H_
