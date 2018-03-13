//
// PROJECT:         Aspia
// FILE:            address_book/open_address_book_dialog.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_ADDRESS_BOOK__OPEN_ADDRESS_BOOK_DIALOG_H
#define _ASPIA_ADDRESS_BOOK__OPEN_ADDRESS_BOOK_DIALOG_H

#include <QtWidgets/QDialog>

#include "base/macros.h"
#include "proto/address_book.pb.h"
#include "qt/ui_open_address_book_dialog.h"

namespace aspia {

class OpenAddressBookDialog : public QDialog
{
    Q_OBJECT

public:
    OpenAddressBookDialog(QWidget* parent, proto::AddressBook::EncryptionType encryption_type);
    ~OpenAddressBookDialog() = default;

    QString Password() const;

private slots:
    void OnShowPasswordButtonToggled(bool checked);
    void OnButtonBoxClicked(QAbstractButton* button);

private:
    Ui::OpenAddressBookDialog ui;

    DISALLOW_COPY_AND_ASSIGN(OpenAddressBookDialog);
};

} // namespace aspia

#endif // _ASPIA_ADDRESS_BOOK__OPEN_ADDRESS_BOOK_DIALOG_H
