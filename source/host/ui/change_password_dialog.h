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

#ifndef HOST_UI_CHANGE_PASSWORD_DIALOG_H
#define HOST_UI_CHANGE_PASSWORD_DIALOG_H

#include "ui_change_password_dialog.h"

namespace host {

class ChangePasswordDialog final : public QDialog
{
    Q_OBJECT

public:
    enum class Mode { CREATE_NEW_PASSWORD, CHANGE_PASSWORD };
    Q_ENUM(Mode)

    explicit ChangePasswordDialog(Mode mode, QWidget* parent = nullptr);
    ~ChangePasswordDialog() final;

    QString oldPassword() const;
    QString newPassword() const;

private slots:
    void onButtonBoxClicked(QAbstractButton* button);

private:
    Ui::ChangePasswordDialog ui;
    const Mode mode_;
};

} // namespace host

#endif // HOST_UI_CHANGE_PASSWORD_DIALOG_H
