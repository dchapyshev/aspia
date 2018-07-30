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

#ifndef ASPIA_CONSOLE__ADDRESS_BOOK_DIALOG_H_
#define ASPIA_CONSOLE__ADDRESS_BOOK_DIALOG_H_

#include "base/macros_magic.h"
#include "protocol/address_book.pb.h"
#include "ui_address_book_dialog.h"

namespace aspia {

class AddressBookDialog : public QDialog
{
    Q_OBJECT

public:
    AddressBookDialog(QWidget* parent,
                      proto::address_book::File* file,
                      proto::address_book::Data* data,
                      QByteArray* key);
    ~AddressBookDialog() = default;

protected:
    // QDialog implementation.
    bool eventFilter(QObject* object, QEvent* event) override;

private slots:
    void buttonBoxClicked(QAbstractButton* button);
    void encryptionTypedChanged(int item_index);
    void hashingRoundsChanged(int value);
    void hashingSaltChanged(int value);

private:
    void setPasswordChanged();
    void showError(const QString& message);

    Ui::AddressBookDialog ui;

    proto::address_book::File* file_;
    proto::address_book::Data* data_;
    QByteArray* key_;

    bool password_changed_ = true;
    bool value_reverting_ = false;

    DISALLOW_COPY_AND_ASSIGN(AddressBookDialog);
};

} // namespace aspia

#endif // ASPIA_CONSOLE__ADDRESS_BOOK_DIALOG_H_
