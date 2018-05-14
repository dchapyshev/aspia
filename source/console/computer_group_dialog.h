//
// PROJECT:         Aspia
// FILE:            console/computer_group_dialog.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CONSOLE__COMPUTER_GROUP_DIALOG_H
#define _ASPIA_CONSOLE__COMPUTER_GROUP_DIALOG_H

#include "console/computer_group_item.h"
#include "ui_computer_group_dialog.h"

namespace aspia {

class ComputerGroupDialog : public QDialog
{
    Q_OBJECT

public:
    enum Mode { CreateComputerGroup, ModifyComputerGroup };

    ComputerGroupDialog(QWidget* parent,
                        Mode mode,
                        proto::address_book::ComputerGroup* computer_group,
                        proto::address_book::ComputerGroup* parent_computer_group);
    ~ComputerGroupDialog();

private slots:
    void buttonBoxClicked(QAbstractButton* button);

private:
    void showError(const QString& message);

    Ui::ComputerGroupDialog ui;

    const Mode mode_;
    proto::address_book::ComputerGroup* computer_group_;

    Q_DISABLE_COPY(ComputerGroupDialog)
};

} // namespace aspia

#endif // _ASPIA_CONSOLE__COMPUTER_GROUP_DIALOG_H
