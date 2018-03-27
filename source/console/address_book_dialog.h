//
// PROJECT:         Aspia
// FILE:            console/address_book_dialog.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CONSOLE__ADDRESS_BOOK_DIALOG_H
#define _ASPIA_CONSOLE__ADDRESS_BOOK_DIALOG_H

#include <QDialog>

#include "console/address_book.h"
#include "qt/ui_address_book_dialog.h"

namespace aspia {

class AddressBookDialog : public QDialog
{
    Q_OBJECT

public:
    AddressBookDialog(QWidget* parent,
                      AddressBook* address_book,
                      proto::AddressBook::EncryptionType* encryption_type,
                      QString* password);
    ~AddressBookDialog() = default;

private slots:
    void OnButtonBoxClicked(QAbstractButton* button);
    void OnEncryptionTypedChanged(int item_index);
    void OnShowPasswordButtonToggled(bool checked);

private:
    void ShowError(const QString& message);

    Ui::AddressBookDialog ui;
    AddressBook* address_book_;
    proto::AddressBook::EncryptionType* encryption_type_;
    QString* password_;

    Q_DISABLE_COPY(AddressBookDialog)
};

} // namespace aspia

#endif // _ASPIA_CONSOLE__ADDRESS_BOOK_DIALOG_H
