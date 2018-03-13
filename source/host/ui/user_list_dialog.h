//
// PROJECT:         Aspia
// FILE:            host/ui/user_list_dialog.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_HOST__UI__USER_LIST_DIALOG_H
#define _ASPIA_HOST__UI__USER_LIST_DIALOG_H

#include "base/macros.h"
#include "host/user_list.h"
#include "qt/ui_user_list_dialog.h"

namespace aspia {

class UserListDialog : public QDialog
{
    Q_OBJECT

public:
    UserListDialog(QWidget* parent = nullptr);
    ~UserListDialog() = default;

private slots:
    void onContextMenu(const QPoint& point);
    void onCurrentItemChanged(QTreeWidgetItem* current, QTreeWidgetItem* previous);
    void onAddUser();
    void onModifyUser();
    void onDeleteUser();
    void onButtonBoxClicked(QAbstractButton* button);

private:
    void reloadUserList();

    Ui::UserListDialog ui;
    UserList user_list_;

    DISALLOW_COPY_AND_ASSIGN(UserListDialog);
};

} // namespace aspia

#endif // _ASPIA_HOST__UI__USER_LIST_DIALOG_H
