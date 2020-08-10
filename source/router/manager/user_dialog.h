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

#ifndef ROUTER__MANAGER__USER_DIALOG_H
#define ROUTER__MANAGER__USER_DIALOG_H

#include "base/macros_magic.h"
#include "proto/router.pb.h"
#include "ui_user_dialog.h"

namespace router {

class UserDialog : public QDialog
{
    Q_OBJECT

public:
    UserDialog(std::unique_ptr<proto::User>,
               const std::vector<std::string>& users,
               QWidget* parent);
    ~UserDialog();

    const proto::User& user() const;

protected:
    // QDialog implementation.
    bool eventFilter(QObject* object, QEvent* event) override;

private:
    void onButtonBoxClicked(QAbstractButton* button);
    void setAccountChanged(bool changed);
    static QString sessionTypeToString(proto::RouterSession session_type);

    Ui::UserDialog ui;

    std::unique_ptr<proto::User> user_;
    std::vector<std::string> users_;
    bool account_changed_ = true;

    DISALLOW_COPY_AND_ASSIGN(UserDialog);
};

} // namespace router

#endif // ROUTER__MANAGER__USER_DIALOG_H
