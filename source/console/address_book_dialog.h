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

#ifndef CONSOLE_ADDRESS_BOOK_DIALOG_H
#define CONSOLE_ADDRESS_BOOK_DIALOG_H

#include "proto/address_book.h"
#include "ui_address_book_dialog.h"

namespace console {

class AddressBookDialog final : public QDialog
{
    Q_OBJECT

public:
    AddressBookDialog(QWidget* parent,
                      const QString& file_path,
                      proto::address_book::File* file,
                      proto::address_book::Data* data,
                      QByteArray* key);
    ~AddressBookDialog() final;

protected:
    // QDialog implementation.
    bool eventFilter(QObject* object, QEvent* event) final;
    void closeEvent(QCloseEvent* event) final;
    void keyPressEvent(QKeyEvent* event) final;

private slots:
    void buttonBoxClicked(QAbstractButton* button);
    void encryptionTypedChanged(int item_index);
    void onTabChanged(QTreeWidgetItem* current);

private:
    void setPasswordChanged();
    void showError(const QString& message);
    void showTab(int type);
    bool saveChanges();

    Ui::AddressBookDialog ui;
    QWidgetList tabs_;

    proto::address_book::File* file_;
    proto::address_book::Data* data_;
    QByteArray* key_;

    bool password_changed_ = true;
    bool value_reverting_ = false;

    Q_DISABLE_COPY(AddressBookDialog)
};

} // namespace console

#endif // CONSOLE_ADDRESS_BOOK_DIALOG_H
