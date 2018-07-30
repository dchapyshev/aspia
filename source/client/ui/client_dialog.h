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

#ifndef ASPIA_CLIENT__UI__CLIENT_DIALOG_H_
#define ASPIA_CLIENT__UI__CLIENT_DIALOG_H_

#include "base/macros_magic.h"
#include "protocol/address_book.pb.h"
#include "ui_client_dialog.h"

namespace aspia {

class ClientDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ClientDialog(QWidget* parent = nullptr);
    ~ClientDialog() = default;

    const proto::address_book::Computer& computer() const { return computer_; }

private slots:
    void sessionTypeChanged(int item_index);
    void sessionConfigButtonPressed();
    void connectButtonPressed();

private:
    Ui::ClientDialog ui;
    proto::address_book::Computer computer_;

    DISALLOW_COPY_AND_ASSIGN(ClientDialog);
};

} // namespace aspia

#endif // ASPIA_CLIENT__UI__CLIENT_DIALOG_H_
