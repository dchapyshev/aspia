//
// PROJECT:         Aspia
// FILE:            client/ui/system_info_window.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/ui/system_info_window.h"

#include <QLineEdit>
#include <QMenu>

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

    menu_ = new QMenu;
    menu_->addAction(ui.action_save_current);
    menu_->addAction(ui.action_save_selected);
    menu_->addAction(ui.action_save_all);

    ui.action_save_report->setMenu(menu_);

    search_edit_ = new QLineEdit(ui.toolbar);
    ui.toolbar->addWidget(search_edit_);
}

void SystemInfoWindow::closeEvent(QCloseEvent* event)
{
    emit windowClose();
    QWidget::closeEvent(event);
}

} // namespace aspia
