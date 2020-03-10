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

#ifndef ROUTER__UI__USER_DIALOG_H
#define ROUTER__UI__USER_DIALOG_H

#include "base/macros_magic.h"
#include "ui_user_dialog.h"

namespace router {

class UserDialog : public QDialog
{
    Q_OBJECT

public:
    explicit UserDialog(QWidget* parent = nullptr);
    ~UserDialog();

    void setUserName(const QString& username);
    QString userName() const;
    QString password() const;
    void setEnabled(bool enable);
    bool isEnabled() const;

private:
    void onButtonBoxClicked(QAbstractButton* button);

    Ui::UserDialog ui;

    DISALLOW_COPY_AND_ASSIGN(UserDialog);
};

} // namespace router

#endif // ROUTER__UI__USER_DIALOG_H
