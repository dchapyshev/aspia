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

#ifndef ASPIA_CLIENT__UI__CLIENT_DIALOG_H
#define ASPIA_CLIENT__UI__CLIENT_DIALOG_H

#include "base/macros_magic.h"
#include "client/connect_data.h"
#include "ui_client_dialog.h"

namespace client {

class ClientDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ClientDialog(QWidget* parent = nullptr);
    ~ClientDialog();

    const ConnectData& connectData() const { return connect_data_; }

private slots:
    void sessionTypeChanged(int item_index);
    void sessionConfigButtonPressed();
    void connectButtonPressed();

private:
    Ui::ClientDialog ui;
    ConnectData connect_data_;

    DISALLOW_COPY_AND_ASSIGN(ClientDialog);
};

} // namespace client

#endif // ASPIA_CLIENT__UI__CLIENT_DIALOG_H
