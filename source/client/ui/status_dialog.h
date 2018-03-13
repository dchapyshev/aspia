//
// PROJECT:         Aspia
// FILE:            client/ui/status_dialog.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__UI__STATUS_DIALOG_H
#define _ASPIA_CLIENT__UI__STATUS_DIALOG_H

#include <QDialog>

#include "base/macros.h"
#include "qt/ui_status_dialog.h"

namespace aspia {

class StatusDialog : public QDialog
{
    Q_OBJECT

public:
    StatusDialog(QWidget* parent = nullptr);
    ~StatusDialog() = default;

public slots:
    // Adds a message to the status dialog. If the dialog is hidden, it also shows it.
    void addStatus(const QString& status);

private:
    Ui::StatusDialog ui;

    DISALLOW_COPY_AND_ASSIGN(StatusDialog);
};

} // namespace aspia

#endif // _ASPIA_CLIENT__UI__STATUS_DIALOG_H
