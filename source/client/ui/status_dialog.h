//
// PROJECT:         Aspia
// FILE:            client/ui/status_dialog.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__UI__STATUS_DIALOG_H
#define _ASPIA_CLIENT__UI__STATUS_DIALOG_H

#include "ui_status_dialog.h"

namespace aspia {

class StatusDialog : public QDialog
{
    Q_OBJECT

public:
    explicit StatusDialog(QWidget* parent = nullptr);
    ~StatusDialog();

public slots:
    // Adds a message to the status dialog. If the dialog is hidden, it also shows it.
    void addStatus(const QString& status);

private:
    Ui::StatusDialog ui;

    Q_DISABLE_COPY(StatusDialog)
};

} // namespace aspia

#endif // _ASPIA_CLIENT__UI__STATUS_DIALOG_H
