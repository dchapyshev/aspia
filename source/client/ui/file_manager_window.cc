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

#include "client/ui/file_remove_dialog.h"
#include "client/ui/file_transfer_dialog.h"
#include "host/file_request.h"

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

    connect(ui.local_panel, &FilePanel::removeItems, this, &FileManagerWindow::removeItems);
    connect(ui.remote_panel, &FilePanel::removeItems, this, &FileManagerWindow::removeItems);
    connect(ui.local_panel, &FilePanel::sendItems, this, &FileManagerWindow::sendItems);
    connect(ui.remote_panel, &FilePanel::sendItems, this, &FileManagerWindow::sendItems);
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

void FileManagerWindow::removeItems(FilePanel* sender, QList<FileRemover::Item> items)
{
    FileRemoveDialog* progress_dialog = new FileRemoveDialog(this);
    FileRemover* remover = new FileRemover(progress_dialog);

    if (sender == ui.local_panel)
    {
        connect(remover, &FileRemover::request, this, &FileManagerWindow::localRequest);
    }
    else
    {
        Q_ASSERT(sender == ui.remote_panel);
        connect(remover, &FileRemover::request, this, &FileManagerWindow::remoteRequest);
    }

    connect(remover, &FileRemover::started, progress_dialog, &FileRemoveDialog::open);
    connect(remover, &FileRemover::finished, progress_dialog, &FileRemoveDialog::close);
    connect(remover, &FileRemover::finished, sender, &FilePanel::refresh);

    connect(remover, &FileRemover::progressChanged,
            progress_dialog, &FileRemoveDialog::setProgress);

    connect(remover, &FileRemover::error, progress_dialog, &FileRemoveDialog::showError);

    connect(progress_dialog, &FileRemoveDialog::finished, [progress_dialog](int /* result */)
    {
        progress_dialog->deleteLater();
    });

    connect(this, &FileManagerWindow::windowClose, progress_dialog, &FileRemoveDialog::close);

    remover->start(sender->currentPath(), items);
}

void FileManagerWindow::sendItems(FilePanel* sender, QList<FileTransfer::Item> items)
{
    FileTransferDialog* progress_dialog = new FileTransferDialog(this);
    FileTransfer* transfer;

    QString source_path;
    QString target_path;

    if (sender == ui.local_panel)
    {
        transfer = new FileTransfer(FileTransfer::Uploader, progress_dialog);

        source_path = ui.local_panel->currentPath();
        target_path = ui.remote_panel->currentPath();

        connect(transfer, &FileTransfer::finished, ui.remote_panel, &FilePanel::refresh);
    }
    else
    {
        Q_ASSERT(sender == ui.remote_panel);

        transfer = new FileTransfer(FileTransfer::Downloader, progress_dialog);

        source_path = ui.remote_panel->currentPath();
        target_path = ui.local_panel->currentPath();

        connect(transfer, &FileTransfer::finished, ui.local_panel, &FilePanel::refresh);
    }

    connect(transfer, &FileTransfer::started, progress_dialog, &FileTransferDialog::open);
    connect(transfer, &FileTransfer::finished, progress_dialog, &FileTransferDialog::close);

    connect(transfer, &FileTransfer::currentItemChanged,
            progress_dialog, &FileTransferDialog::setCurrentItem);
    connect(transfer, &FileTransfer::progressChanged,
            progress_dialog, &FileTransferDialog::setProgress);

    connect(transfer, &FileTransfer::error, progress_dialog, &FileTransferDialog::showError);
    connect(transfer, &FileTransfer::localRequest, this, &FileManagerWindow::localRequest);
    connect(transfer, &FileTransfer::remoteRequest, this, &FileManagerWindow::remoteRequest);

    connect(progress_dialog, &FileTransferDialog::finished, [progress_dialog](int /* result */)
    {
        progress_dialog->deleteLater();
    });

    connect(this, &FileManagerWindow::windowClose, progress_dialog, &FileTransferDialog::close);

    transfer->start(source_path, target_path, items);
}

} // namespace aspia
