//
// PROJECT:         Aspia
// FILE:            client/ui/authorization_dialog.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__UI__AUTHORIZATION_DIALOG_H
#define _ASPIA_CLIENT__UI__AUTHORIZATION_DIALOG_H

#include "ui_authorization_dialog.h"

namespace aspia {

class AuthorizationDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AuthorizationDialog(QWidget* parent = nullptr);
    ~AuthorizationDialog();

    QString userName() const;
    void setUserName(const QString& username);

    QString password() const;
    void setPassword(const QString& password);

protected:
    void showEvent(QShowEvent* event) override;

private slots:
    void onShowPasswordButtonToggled(bool checked);
    void onButtonBoxClicked(QAbstractButton* button);

private:
    Ui::AuthorizationDialog ui;

    Q_DISABLE_COPY(AuthorizationDialog)
};

} // namespace aspia

#endif // _ASPIA_CLIENT__UI__AUTHORIZATION_DIALOG_H
