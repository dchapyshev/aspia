//
// PROJECT:         Aspia
// FILE:            host/ui/user_list_dialog.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_HOST__UI__USER_LIST_DIALOG_H
#define _ASPIA_HOST__UI__USER_LIST_DIALOG_H

#include "host/user.h"
#include "ui_user_list_dialog.h"

namespace aspia {

class UserListDialog : public QDialog
{
    Q_OBJECT

public:
    explicit UserListDialog(QWidget* parent = nullptr);
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
    QList<User> user_list_;

    Q_DISABLE_COPY(UserListDialog)
};

} // namespace aspia

#endif // _ASPIA_HOST__UI__USER_LIST_DIALOG_H
