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
                      proto::address_book::File* file,
                      proto::address_book::Data* data,
                      QByteArray* key);
    ~AddressBookDialog();

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

    Q_DISABLE_COPY(AddressBookDialog)
};

} // namespace aspia

#endif // _ASPIA_CONSOLE__ADDRESS_BOOK_DIALOG_H
