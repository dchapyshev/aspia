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

#ifndef HOST_UI_USER_DIALOG_H
#define HOST_UI_USER_DIALOG_H

#include <QDialog>

#include <memory>

#include "base/peer/user.h"

class QAbstractButton;

namespace Ui {
class UserDialog;
} // namespace Ui

class UserDialog final : public QDialog
{
    Q_OBJECT

public:
    // entry_id == 0 - Create a new user.
    // entry_id != 0 - Edit an existing user.
    explicit UserDialog(qint64 entry_id, QWidget* parent);
    ~UserDialog() final;

protected:
    // QDialog implementation.
    bool eventFilter(QObject* object, QEvent* event) final;

private slots:
    void onCheckAllButtonPressed();
    void onCheckNoneButtonPressed();
    void onButtonBoxClicked(QAbstractButton* button);

private:
    void setAccountChanged(bool changed);

    std::unique_ptr<Ui::UserDialog> ui;
    qint64 entry_id_;
    User user_;
    bool account_changed_ = true;

    Q_DISABLE_COPY_MOVE(UserDialog)
};

#endif // HOST_UI_USER_DIALOG_H
