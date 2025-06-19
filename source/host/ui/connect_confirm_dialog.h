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

#ifndef HOST_UI_CONNECT_CONFIRM_DIALOG_H
#define HOST_UI_CONNECT_CONFIRM_DIALOG_H

#include <QDialog>

#include "proto/host_internal.h"
#include "ui_connect_confirm_dialog.h"

namespace host {

class ConnectConfirmDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit ConnectConfirmDialog(const proto::internal::ConnectConfirmationRequest& request,
                                  QWidget* parent = nullptr);
    ~ConnectConfirmDialog() final;

private slots:
    void onButtonBoxClicked(QAbstractButton* button);
    void onTimeout();

private:
    void updateMessage();

    Ui::ConnectConfirmDialog ui;
    QTimer* timer_ = nullptr;

    QString message_;
    QString question_;

    bool auto_accept_ = false;
    int time_seconds_ = 0;
};

} // namespace host

#endif // HOST_UI_CONNECT_CONFIRM_DIALOG_H
