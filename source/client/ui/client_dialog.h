//
// PROJECT:         Aspia
// FILE:            client/ui/client_dialog.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__UI__CLIENT_DIALOG_H
#define _ASPIA_CLIENT__UI__CLIENT_DIALOG_H

#include <QDialog>

#include "proto/computer.pb.h"
#include "qt/ui_client_dialog.h"

namespace aspia {

class ClientDialog : public QDialog
{
    Q_OBJECT

public:
    ClientDialog(QWidget* parent = nullptr);
    ~ClientDialog() = default;

private slots:
    void OnSessionTypeChanged(int item_index);
    void OnSessionConfigButtonPressed();
    void OnConnectButtonPressed();

private:
    void SetDefaultConfig();

    Ui::ClientDialog ui;
    proto::Computer computer_;

    Q_DISABLE_COPY(ClientDialog)
};

} // namespace aspia

#endif // _ASPIA_CLIENT__UI__CLIENT_DIALOG_H
