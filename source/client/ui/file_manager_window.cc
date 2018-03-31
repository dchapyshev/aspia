//
// PROJECT:         Aspia
// FILE:            client/ui/file_manager_window.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/ui/file_manager_window.h"

#include <QCoreApplication>
#include <QDebug>
#include <QMessageBox>

#include "client/file_request.h"

namespace aspia {

FileManagerWindow::FileManagerWindow(proto::Computer* computer, QWidget* parent)
    : QWidget(parent)
{
    ui.setupUi(this);

    QString computer_name;
    if (!computer->name().empty())
        computer_name = QString::fromStdString(computer->name());
    else
        computer_name = QString::fromStdString(computer->address());

    setWindowTitle(tr("%1 - Aspia File Transfer").arg(computer_name));

    ui.local_panel->setPanelName(tr("Local Computer"));
    ui.remote_panel->setPanelName(tr("Remote Computer"));

    QList<int> sizes;
    sizes.push_back(width() / 2);
    sizes.push_back(width() / 2);

    ui.splitter->setSizes(sizes);

    connect(ui.local_panel, &FilePanel::request, this, &FileManagerWindow::localRequest);
    connect(ui.remote_panel, &FilePanel::request, this, &FileManagerWindow::remoteRequest);
}

void FileManagerWindow::refresh()
{
    ui.local_panel->refresh();
    ui.remote_panel->refresh();
}

void FileManagerWindow::closeEvent(QCloseEvent* event)
{
    emit windowClose();
    QWidget::closeEvent(event);
}

} // namespace aspia
