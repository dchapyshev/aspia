//
// PROJECT:         Aspia
// FILE:            client/ui/authorization_dialog.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__UI__AUTHORIZATION_DIALOG_H
#define _ASPIA_CLIENT__UI__AUTHORIZATION_DIALOG_H

#include "ui_authorization_dialog.h"

namespace aspia {

namespace proto {
class Computer;
} // namespace proto

class AuthorizationDialog : public QDialog
{
    Q_OBJECT

public:
    AuthorizationDialog(proto::Computer* computer, QWidget* parent);
    ~AuthorizationDialog();

private slots:
    void OnShowPasswordButtonToggled(bool checked);
    void OnButtonBoxClicked(QAbstractButton* button);

private:
    Ui::AuthorizationDialog ui;
    proto::Computer* computer_;

    Q_DISABLE_COPY(AuthorizationDialog)
};

} // namespace aspia

#endif // _ASPIA_CLIENT__UI__AUTHORIZATION_DIALOG_H
