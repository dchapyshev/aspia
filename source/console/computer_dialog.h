//
// PROJECT:         Aspia
// FILE:            console/computer_dialog.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CONSOLE__COMPUTER_DIALOG_H
#define _ASPIA_CONSOLE__COMPUTER_DIALOG_H

#include "console/computer.h"
#include "protocol/address_book.pb.h"
#include "ui_computer_dialog.h"

namespace aspia {

class ComputerDialog : public QDialog
{
    Q_OBJECT

public:
    ComputerDialog(QWidget* parent, Computer* computer, ComputerGroup* parent_group);
    ~ComputerDialog();

private slots:
    void sessionTypeChanged(int item_index);
    void showPasswordButtonToggled(bool checked);
    void sessionConfigButtonPressed();
    void buttonBoxClicked(QAbstractButton* button);

private:
    void showError(const QString& message);

    Ui::ComputerDialog ui;
    Computer* computer_;

    Q_DISABLE_COPY(ComputerDialog)
};

} // namespace aspia

#endif // _ASPIA_CONSOLE__COMPUTER_DIALOG_H
