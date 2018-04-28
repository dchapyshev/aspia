//
// PROJECT:         Aspia
// FILE:            client/ui/client_dialog.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__UI__CLIENT_DIALOG_H
#define _ASPIA_CLIENT__UI__CLIENT_DIALOG_H

#include "protocol/address_book.pb.h"
#include "ui_client_dialog.h"

namespace aspia {

class ClientDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ClientDialog(QWidget* parent = nullptr);
    ~ClientDialog();

    proto::address_book::Computer computer() const { return computer_; }

private slots:
    void sessionTypeChanged(int item_index);
    void sessionConfigButtonPressed();
    void connectButtonPressed();

private:
    void setDefaultConfig();

    Ui::ClientDialog ui;
    proto::address_book::Computer computer_;

    Q_DISABLE_COPY(ClientDialog)
};

} // namespace aspia

#endif // _ASPIA_CLIENT__UI__CLIENT_DIALOG_H
