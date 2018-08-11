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

#include "client/ui/address_bar_model.h"
#include "client/ui/file_remove_dialog.h"
#include "client/ui/file_transfer_dialog.h"
#include "client/ui/file_manager_settings.h"
#include "client/ui/file_mime_data.h"

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

    FileManagerSettings settings;
    restoreGeometry(settings.windowGeometry());

    ui.splitter->restoreState(settings.splitterState());

    QString mime_type = FileMimeData::createMimeType();

    ui.local_panel->setPanelName(tr("Local Computer"));
    ui.local_panel->setMimeType(mime_type);
    ui.local_panel->setDriveListState(settings.localDriveListState());
    ui.local_panel->setFileListState(settings.localFileListState());

    ui.remote_panel->setPanelName(tr("Remote Computer"));
    ui.remote_panel->setMimeType(mime_type);
    ui.remote_panel->setDriveListState(settings.remoteDriveListState());
    ui.remote_panel->setFileListState(settings.remoteFileListState());

    connect(ui.local_panel, &FilePanel::removeItems, this, &FileManagerWindow::removeItems);
    connect(ui.remote_panel, &FilePanel::removeItems, this, &FileManagerWindow::removeItems);
    connect(ui.local_panel, &FilePanel::sendItems, this, &FileManagerWindow::sendItems);
    connect(ui.remote_panel, &FilePanel::sendItems, this, &FileManagerWindow::sendItems);
    connect(ui.local_panel, &FilePanel::receiveItems, this, &FileManagerWindow::receiveItems);
    connect(ui.remote_panel, &FilePanel::receiveItems, this, &FileManagerWindow::receiveItems);
    connect(ui.local_panel, &FilePanel::newRequest, this, &FileManagerWindow::localRequest);
    connect(ui.remote_panel, &FilePanel::newRequest, this, &FileManagerWindow::remoteRequest);
    connect(ui.local_panel, &FilePanel::pathChanged, this, &FileManagerWindow::onPathChanged);
    connect(ui.remote_panel, &FilePanel::pathChanged, this, &FileManagerWindow::onPathChanged);
}

void FileManagerWindow::refresh()
{
    ui.local_panel->refresh();
    ui.remote_panel->refresh();
}

void FileManagerWindow::closeEvent(QCloseEvent* event)
{
    FileManagerSettings settings;

    settings.setWindowGeometry(saveGeometry());
    settings.setSplitterState(ui.splitter->saveState());

    ui.local_panel->updateState();
    settings.setLocalDriveListState(ui.local_panel->driveListState());
    settings.setLocalFileListState(ui.local_panel->fileListState());

    ui.remote_panel->updateState();
    settings.setRemoteDriveListState(ui.remote_panel->driveListState());
    settings.setRemoteFileListState(ui.remote_panel->fileListState());

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

    connect(progress_dialog, &FileRemoveDialog::finished,
            progress_dialog, &FileRemoveDialog::deleteLater);

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
    FileTransferDialog* dialog = new FileTransferDialog(this);
    FileTransfer* transfer = new FileTransfer(type, dialog);

    if (type == FileTransfer::Uploader)
    {
        connect(transfer, &FileTransfer::finished, ui.remote_panel, &FilePanel::refresh);
    }
    else
    {
        Q_ASSERT(type == FileTransfer::Downloader);
        connect(transfer, &FileTransfer::finished, ui.local_panel, &FilePanel::refresh);
    }

    connect(transfer, &FileTransfer::started, dialog, &FileTransferDialog::open);
    connect(transfer, &FileTransfer::finished, dialog, &FileTransferDialog::onTransferFinished);

    connect(transfer, &FileTransfer::currentItemChanged, dialog, &FileTransferDialog::setCurrentItem);
    connect(transfer, &FileTransfer::progressChanged, dialog, &FileTransferDialog::setProgress);

    connect(transfer, &FileTransfer::error, dialog, &FileTransferDialog::showError);
    connect(transfer, &FileTransfer::localRequest, this, &FileManagerWindow::localRequest);
    connect(transfer, &FileTransfer::remoteRequest, this, &FileManagerWindow::remoteRequest);

    connect(dialog, &FileTransferDialog::transferCanceled, transfer, &FileTransfer::stop);
    connect(dialog, &FileTransferDialog::finished, dialog, &FileTransferDialog::deleteLater);

    connect(this, &FileManagerWindow::windowClose, dialog, &FileTransferDialog::onTransferFinished);

    transfer->start(source_path, target_path, items);
}

void FileManagerWindow::onPathChanged(FilePanel* sender, const QString& path)
{
    bool allow = path != AddressBarModel::computerPath();

    if (sender == ui.local_panel)
    {
        ui.remote_panel->setTransferAllowed(allow);
    }
    else
    {
        Q_ASSERT(sender == ui.remote_panel);
        ui.local_panel->setTransferAllowed(allow);

    }
}

} // namespace aspia
