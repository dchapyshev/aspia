//
// PROJECT:         Aspia
// FILE:            host/ui/user_dialog.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_HOST__UI__USER_DIALOG_H
#define _ASPIA_HOST__UI__USER_DIALOG_H

#include "host/user_list.h"
#include "ui_user_dialog.h"

namespace aspia {

class UserDialog : public QDialog
{
    Q_OBJECT

public:
    UserDialog(UserList* user_list, User* user, QWidget* parent = nullptr);
    ~UserDialog() = default;

protected:
    // QDialog implementation.
    bool eventFilter(QObject* object, QEvent* event) override;

private slots:
    void onCheckAllButtonPressed();
    void onCheckNoneButtonPressed();
    void onButtonBoxClicked(QAbstractButton* button);

private:
    Ui::UserDialog ui;
    UserList* user_list_;
    User* user_;

    Q_DISABLE_COPY(UserDialog)
};

} // namespace aspia

#endif // _ASPIA_HOST__UI__USER_DIALOG_H
