//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#include "client/ui/file_manager_window.h"

#include <QDebug>

#include "client/ui/file_remove_dialog.h"
#include "client/ui/file_transfer_dialog.h"

namespace aspia {

FileManagerWindow::FileManagerWindow(ConnectData* connect_data, QWidget* parent)
    : QWidget(parent)
{
    ui.setupUi(this);

    QString computer_name;
    if (!connect_data->computerName().isEmpty())
        computer_name = connect_data->computerName();
    else
        computer_name = connect_data->address();

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
    connect(ui.local_panel, &FilePanel::receiveItems, this, &FileManagerWindow::receiveItems);
    connect(ui.remote_panel, &FilePanel::receiveItems, this, &FileManagerWindow::receiveItems);
    connect(ui.local_panel, &FilePanel::newRequest, this, &FileManagerWindow::localRequest);
    connect(ui.remote_panel, &FilePanel::newRequest, this, &FileManagerWindow::remoteRequest);
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

void FileManagerWindow::removeItems(FilePanel* sender, const QList<FileRemover::Item>& items)
{
    FileRemoveDialog* progress_dialog = new FileRemoveDialog(this);
    FileRemover* remover = new FileRemover(progress_dialog);

    if (sender == ui.local_panel)
    {
        connect(remover, &FileRemover::newRequest, this, &FileManagerWindow::localRequest);
    }
    else
    {
        Q_ASSERT(sender == ui.remote_panel);
        connect(remover, &FileRemover::newRequest, this, &FileManagerWindow::remoteRequest);
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

void FileManagerWindow::sendItems(FilePanel* sender, const QList<FileTransfer::Item>& items)
{
    if (sender == ui.local_panel)
    {
        transferItems(FileTransfer::Uploader,
                      ui.local_panel->currentPath(),
                      ui.remote_panel->currentPath(),
                      items);
    }
    else
    {
        Q_ASSERT(sender == ui.remote_panel);

        transferItems(FileTransfer::Downloader,
                      ui.remote_panel->currentPath(),
                      ui.local_panel->currentPath(),
                      items);
    }
}

void FileManagerWindow::receiveItems(FilePanel* sender,
                                     const QString& target_folder,
                                     const QList<FileTransfer::Item>& items)
{
    if (sender == ui.local_panel)
    {
        transferItems(FileTransfer::Downloader,
                      ui.remote_panel->currentPath(),
                      target_folder,
                      items);
    }
    else
    {
        Q_ASSERT(sender == ui.remote_panel);

        transferItems(FileTransfer::Uploader,
                      ui.local_panel->currentPath(),
                      target_folder,
                      items);
    }
}

void FileManagerWindow::transferItems(FileTransfer::Type type,
                                      const QString& source_path,
                                      const QString& target_path,
                                      const QList<FileTransfer::Item>& items)
{
    FileTransferDialog* progress_dialog = new FileTransferDialog(this);
    FileTransfer* transfer = new FileTransfer(type, progress_dialog);

    if (type == FileTransfer::Uploader)
    {
        connect(transfer, &FileTransfer::finished, ui.remote_panel, &FilePanel::refresh);
    }
    else
    {
        Q_ASSERT(type == FileTransfer::Downloader);
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

    connect(progress_dialog, &FileTransferDialog::finished,
            progress_dialog, &FileTransferDialog::deleteLater);

    connect(this, &FileManagerWindow::windowClose, progress_dialog, &FileTransferDialog::close);

    transfer->start(source_path, target_path, items);
}

} // namespace aspia
