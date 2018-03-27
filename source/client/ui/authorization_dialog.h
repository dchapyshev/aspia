//
// PROJECT:         Aspia
// FILE:            client/ui/authorization_dialog.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__UI__AUTHORIZATION_DIALOG_H
#define _ASPIA_CLIENT__UI__AUTHORIZATION_DIALOG_H

#include <QCryptographicHash>
#include <QDialog>

#include "proto/computer.pb.h"
#include "qt/ui_authorization_dialog.h"

namespace aspia {

class AuthorizationDialog : public QDialog
{
    Q_OBJECT

public:
    AuthorizationDialog(proto::Computer* computer, QWidget* parent);
    ~AuthorizationDialog() = default;

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
