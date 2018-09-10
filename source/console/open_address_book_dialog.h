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

#ifndef ASPIA_CONSOLE__OPEN_ADDRESS_BOOK_DIALOG_H_
#define ASPIA_CONSOLE__OPEN_ADDRESS_BOOK_DIALOG_H_

#include "base/macros_magic.h"
#include "protocol/address_book.pb.h"
#include "ui_open_address_book_dialog.h"

namespace aspia {

class OpenAddressBookDialog : public QDialog
{
    Q_OBJECT

public:
    OpenAddressBookDialog(QWidget* parent, proto::address_book::EncryptionType encryption_type);
    ~OpenAddressBookDialog() = default;

    QString password() const;

private slots:
    void showPasswordButtonToggled(bool checked);
    void buttonBoxClicked(QAbstractButton* button);

private:
    Ui::OpenAddressBookDialog ui;

    DISALLOW_COPY_AND_ASSIGN(OpenAddressBookDialog);
};

} // namespace aspia

#endif // ASPIA_CONSOLE__OPEN_ADDRESS_BOOK_DIALOG_H_
