//
// PROJECT:         Aspia
// FILE:            address_book/computer_group_dialog.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_ADDRESS_BOOK__COMPUTER_GROUP_DIALOG_H
#define _ASPIA_ADDRESS_BOOK__COMPUTER_GROUP_DIALOG_H

#include <QtWidgets/QDialog>

#include "address_book/computer_group.h"
#include "qt_generated/ui_computer_group_dialog.h"

namespace aspia {

class ComputerGroupDialog : public QDialog
{
    Q_OBJECT

public:
    ComputerGroupDialog(QWidget* parent, ComputerGroup* group, ComputerGroup* parent_group);
    ~ComputerGroupDialog() = default;

private slots:
    void OnButtonBoxClicked(QAbstractButton* button);

private:
    void ShowError(const QString& message);

    Ui::ComputerGroupDialog ui;
    ComputerGroup* group_;

    DISALLOW_COPY_AND_ASSIGN(ComputerGroupDialog);
};

} // namespace aspia

#endif // _ASPIA_ADDRESS_BOOK__COMPUTER_GROUP_DIALOG_H
