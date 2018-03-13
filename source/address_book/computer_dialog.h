//
// PROJECT:         Aspia
// FILE:            address_book/computer_dialog.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_ADDRESS_BOOK__COMPUTER_DIALOG_H
#define _ASPIA_ADDRESS_BOOK__COMPUTER_DIALOG_H

#include "address_book/computer.h"
#include "proto/address_book.pb.h"
#include "qt/ui_computer_dialog.h"

namespace aspia {

class ComputerDialog : public QDialog
{
    Q_OBJECT

public:
    ComputerDialog(QWidget* parent, Computer* computer, ComputerGroup* parent_group);
    ~ComputerDialog() = default;

private slots:
    void OnSessionTypeChanged(int item_index);
    void OnShowPasswordButtonToggled(bool checked);
    void OnSessionConfigButtonPressed();
    void OnButtonBoxClicked(QAbstractButton* button);

private:
    void ShowError(const QString& message);

    Ui::ComputerDialog ui;
    Computer* computer_;

    DISALLOW_COPY_AND_ASSIGN(ComputerDialog);
};

} // namespace aspia

#endif // _ASPIA_ADDRESS_BOOK__COMPUTER_DIALOG_H
