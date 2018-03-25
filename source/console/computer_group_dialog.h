//
// PROJECT:         Aspia
// FILE:            console/computer_group_dialog.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CONSOLE__COMPUTER_GROUP_DIALOG_H
#define _ASPIA_CONSOLE__COMPUTER_GROUP_DIALOG_H

#include <QDialog>

#include "console/computer_group.h"
#include "qt/ui_computer_group_dialog.h"

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

#endif // _ASPIA_CONSOLE__COMPUTER_GROUP_DIALOG_H
