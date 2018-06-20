//
// PROJECT:         Aspia
// FILE:            client/ui/status_dialog.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/ui/status_dialog.h"

#include <QTime>

namespace aspia {

StatusDialog::StatusDialog(QWidget* parent)
    : QDialog(parent)
{
    ui.setupUi(this);

    connect(ui.button_cancel, &QPushButton::pressed, this, &StatusDialog::close);
}

void StatusDialog::addStatus(const QString& status)
{
    if (isHidden())
    {
        show();
        activateWindow();
    }

    ui.edit_status->appendPlainText(
        QString("%1 %2").arg(QTime::currentTime().toString()).arg(status));
}

} // namespace aspia
