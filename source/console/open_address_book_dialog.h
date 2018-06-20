//
// PROJECT:         Aspia
// FILE:            console/open_address_book_dialog.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CONSOLE__OPEN_ADDRESS_BOOK_DIALOG_H
#define _ASPIA_CONSOLE__OPEN_ADDRESS_BOOK_DIALOG_H

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

    Q_DISABLE_COPY(OpenAddressBookDialog)
};

} // namespace aspia

#endif // _ASPIA_CONSOLE__OPEN_ADDRESS_BOOK_DIALOG_H
