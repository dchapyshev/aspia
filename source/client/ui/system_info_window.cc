//
// PROJECT:         Aspia
// FILE:            client/ui/system_info_window.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/ui/system_info_window.h"

namespace aspia {

SystemInfoWindow::SystemInfoWindow(ConnectData* connect_data, QWidget* parent)
    : QWidget(parent)
{
    ui.setupUi(this);

    QString computer_name;
    if (!connect_data->computerName().isEmpty())
        computer_name = connect_data->computerName();
    else
        computer_name = connect_data->address();

    setWindowTitle(tr("%1 - Aspia System Information").arg(computer_name));
}

void SystemInfoWindow::closeEvent(QCloseEvent* event)
{
    emit windowClose();
    QWidget::closeEvent(event);
}

} // namespace aspia
