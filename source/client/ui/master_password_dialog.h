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

#ifndef CLIENT_UI_MASTER_PASSWORD_DIALOG_H
#define CLIENT_UI_MASTER_PASSWORD_DIALOG_H

#include "ui_master_password_dialog.h"

#include <QDialog>

class QAbstractButton;

namespace client {

class MasterPasswordDialog final : public QDialog
{
    Q_OBJECT

public:
    enum class Mode { SET, CHANGE, REMOVE };
    Q_ENUM(Mode)

    explicit MasterPasswordDialog(Mode mode, QWidget* parent = nullptr);
    ~MasterPasswordDialog() final;

private slots:
    void onButtonBoxClicked(QAbstractButton* button);

private:
    bool applySet();
    bool applyChange();
    bool applyRemove();

    Ui::MasterPasswordDialog ui;
    const Mode mode_;

    Q_DISABLE_COPY_MOVE(MasterPasswordDialog)
};

} // namespace client

#endif // CLIENT_UI_MASTER_PASSWORD_DIALOG_H
