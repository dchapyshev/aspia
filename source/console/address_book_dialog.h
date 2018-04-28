//
// PROJECT:         Aspia
// FILE:            console/address_book_dialog.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CONSOLE__ADDRESS_BOOK_DIALOG_H
#define _ASPIA_CONSOLE__ADDRESS_BOOK_DIALOG_H

#include "protocol/address_book.pb.h"
#include "ui_address_book_dialog.h"

namespace aspia {

class AddressBookDialog : public QDialog
{
    Q_OBJECT

public:
    AddressBookDialog(QWidget* parent,
                      proto::address_book::EncryptionType* encryption_type,
                      QString* password,
                      proto::address_book::ComputerGroup* root_group);
    ~AddressBookDialog();

private slots:
    void buttonBoxClicked(QAbstractButton* button);
    void encryptionTypedChanged(int item_index);
    void showPasswordButtonToggled(bool checked);

private:
    void showError(const QString& message);

    Ui::AddressBookDialog ui;
    proto::address_book::ComputerGroup* root_group_;
    proto::address_book::EncryptionType* encryption_type_;
    QString* password_;

    Q_DISABLE_COPY(AddressBookDialog)
};

} // namespace aspia

#endif // _ASPIA_CONSOLE__ADDRESS_BOOK_DIALOG_H
