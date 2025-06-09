//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "client/ui/file_transfer/file_transfer_session_window.h"

#include "ui_file_transfer_session_window.h"
#include "base/gui_application.h"
#include "base/logging.h"
#include "client/client_file_transfer.h"
#include "client/ui/file_transfer/address_bar_model.h"
#include "client/ui/file_transfer/file_error_code.h"
#include "client/ui/file_transfer/file_remove_dialog.h"
#include "client/ui/file_transfer/file_transfer_dialog.h"
#include "client/ui/file_transfer/file_manager_settings.h"
#include "client/ui/file_transfer/file_mime_data.h"

#include <QMessageBox>

namespace client {

//--------------------------------------------------------------------------------------------------
FileTransferSessionWindow::FileTransferSessionWindow(QWidget* parent)
    : SessionWindow(nullptr, parent),
      ui(std::make_unique<Ui::FileTransferSessionWindow>())
{
    LOG(LS_INFO) << "Ctor";

    ui->setupUi(this);

    FileManagerSettings settings;
    restoreGeometry(settings.windowGeometry());
    restoreState(settings.windowState());

    QString mime_type = FileMimeData::createMimeType();

    initPanel(common::FileTask::Target::LOCAL, tr("Local Computer"), mime_type, ui->local_panel);
    initPanel(common::FileTask::Target::REMOTE, tr("Remote Computer"), mime_type, ui->remote_panel);

    ui->local_panel->setFocus();
}

//--------------------------------------------------------------------------------------------------
FileTransferSessionWindow::~FileTransferSessionWindow()
{
    LOG(LS_INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
Client* FileTransferSessionWindow::createClient()
{
    LOG(LS_INFO) << "Create client";

    ClientFileTransfer* client = new ClientFileTransfer();

    connect(client, &ClientFileTransfer::sig_showSessionWindow,
            this, &FileTransferSessionWindow::onShowWindow,
            Qt::QueuedConnection);
    connect(client, &ClientFileTransfer::sig_errorOccurred,
            this, &FileTransferSessionWindow::onErrorOccurred,
            Qt::QueuedConnection);
    connect(client, &ClientFileTransfer::sig_driveListReply,
            this, &FileTransferSessionWindow::onDriveList,
            Qt::QueuedConnection);
    connect(client, &ClientFileTransfer::sig_fileListReply,
            this, &FileTransferSessionWindow::onFileList,
            Qt::QueuedConnection);
    connect(client, &ClientFileTransfer::sig_createDirectoryReply,
            this, &FileTransferSessionWindow::onCreateDirectory,
            Qt::QueuedConnection);
    connect(client, &ClientFileTransfer::sig_renameReply,
            this, &FileTransferSessionWindow::onRename,
            Qt::QueuedConnection);

    connect(this, &FileTransferSessionWindow::sig_driveListRequest,
            client, &ClientFileTransfer::onDriveListRequest,
            Qt::QueuedConnection);
    connect(this, &FileTransferSessionWindow::sig_fileListRequest,
            client, &ClientFileTransfer::onFileListRequest,
            Qt::QueuedConnection);
    connect(this, &FileTransferSessionWindow::sig_createDirectoryRequest,
            client, &ClientFileTransfer::onCreateDirectoryRequest,
            Qt::QueuedConnection);
    connect(this, &FileTransferSessionWindow::sig_renameRequest,
            client, &ClientFileTransfer::onRenameRequest,
            Qt::QueuedConnection);
    connect(this, &FileTransferSessionWindow::sig_removeRequest,
            client, &ClientFileTransfer::onRemoveRequest,
            Qt::QueuedConnection);
    connect(this, &FileTransferSessionWindow::sig_transferRequest,
            client, &ClientFileTransfer::onTransferRequest,
            Qt::QueuedConnection);

    return client;
}

//--------------------------------------------------------------------------------------------------
void FileTransferSessionWindow::onShowWindow()
{
    LOG(LS_INFO) << "Show window";
    show();
    activateWindow();
    refresh();
}

//--------------------------------------------------------------------------------------------------
void FileTransferSessionWindow::onErrorOccurred(proto::file_transfer::ErrorCode error_code)
{
    LOG(LS_ERROR) << "Session error: " << error_code;
    QMessageBox::warning(this,
                         tr("Warning"),
                         tr("Session error: %1").arg(fileErrorToString(error_code)),
                         QMessageBox::Ok);
    close();
}

//--------------------------------------------------------------------------------------------------
void FileTransferSessionWindow::onDriveList(
    common::FileTask::Target target, proto::file_transfer::ErrorCode error_code,
    const proto::file_transfer::DriveList& drive_list)
{
    if (target == common::FileTask::Target::LOCAL)
    {
        ui->local_panel->onDriveList(error_code, drive_list);
    }
    else
    {
        DCHECK_EQ(target, common::FileTask::Target::REMOTE);
        ui->remote_panel->onDriveList(error_code, drive_list);
    }
}

//--------------------------------------------------------------------------------------------------
void FileTransferSessionWindow::onFileList(
    common::FileTask::Target target, proto::file_transfer::ErrorCode error_code,
    const proto::file_transfer::List& file_list)
{
    if (target == common::FileTask::Target::LOCAL)
    {
        ui->local_panel->onFileList(error_code, file_list);
    }
    else
    {
        DCHECK_EQ(target, common::FileTask::Target::REMOTE);
        ui->remote_panel->onFileList(error_code, file_list);
    }
}

//--------------------------------------------------------------------------------------------------
void FileTransferSessionWindow::onCreateDirectory(
    common::FileTask::Target target, proto::file_transfer::ErrorCode error_code)
{
    if (target == common::FileTask::Target::LOCAL)
    {
        ui->local_panel->onCreateDirectory(error_code);
    }
    else
    {
        DCHECK_EQ(target, common::FileTask::Target::REMOTE);
        ui->remote_panel->onCreateDirectory(error_code);
    }
}

//--------------------------------------------------------------------------------------------------
void FileTransferSessionWindow::onRename(
    common::FileTask::Target target, proto::file_transfer::ErrorCode error_code)
{
    if (target == common::FileTask::Target::LOCAL)
    {
        ui->local_panel->onRename(error_code);
    }
    else
    {
        DCHECK_EQ(target, common::FileTask::Target::REMOTE);
        ui->remote_panel->onRename(error_code);
    }
}

//--------------------------------------------------------------------------------------------------
QByteArray FileTransferSessionWindow::saveState() const
{
    QByteArray buffer;

    {
        QDataStream stream(&buffer, QIODevice::WriteOnly);
        stream.setVersion(QDataStream::Qt_5_12);
        stream << ui->splitter->saveState();
        stream << ui->local_panel->saveState();
        stream << ui->remote_panel->saveState();
    }

    return buffer;
}

//--------------------------------------------------------------------------------------------------
void FileTransferSessionWindow::restoreState(const QByteArray& state)
{
    QDataStream stream(state);
    stream.setVersion(QDataStream::Qt_5_12);

    QByteArray value;

    stream >> value;
    ui->splitter->restoreState(value);

    stream >> value;
    ui->local_panel->restoreState(value);

    stream >> value;
    ui->remote_panel->restoreState(value);
}

//--------------------------------------------------------------------------------------------------
void FileTransferSessionWindow::refresh()
{
    ui->local_panel->refresh();
    ui->remote_panel->refresh();
}

//--------------------------------------------------------------------------------------------------
void FileTransferSessionWindow::onInternalReset()
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
void FileTransferSessionWindow::closeEvent(QCloseEvent* event)
{
    LOG(LS_INFO) << "Close event detected";

    if (transfer_dialog_)
    {
        LOG(LS_INFO) << "Stopping transfer dialog";
        transfer_dialog_->stop();
    }

    if (remove_dialog_)
    {
        LOG(LS_INFO) << "Stopping remove dialog";
        remove_dialog_->stop();
    }

    FileManagerSettings settings;

    settings.setWindowGeometry(saveGeometry());
    settings.setWindowState(saveState());

    SessionWindow::closeEvent(event);
}

//--------------------------------------------------------------------------------------------------
void FileTransferSessionWindow::removeItems(FilePanel* sender, const FileRemover::TaskList& items)
{
    remove_dialog_ = new FileRemoveDialog(this);
    remove_dialog_->setAttribute(Qt::WA_DeleteOnClose);

    connect(remove_dialog_, &FileRemoveDialog::finished, this, [this]()
    {
        refresh();
        activateWindow();
        setFocus();

        ui->local_panel->setEnabled(true);
        ui->remote_panel->setEnabled(true);
    });

    common::FileTask::Target target;

    if (sender == ui->local_panel)
    {
        target = common::FileTask::Target::LOCAL;
    }
    else
    {
        DCHECK_EQ(sender, ui->remote_panel);
        target = common::FileTask::Target::REMOTE;
    }

    ui->local_panel->setEnabled(false);
    ui->remote_panel->setEnabled(false);

    FileRemover* remover = new FileRemover(target, items);
    remover->moveToThread(base::GuiApplication::ioThread());

    connect(remover, &FileRemover::sig_started,
            remove_dialog_, &FileRemoveDialog::start,
            Qt::QueuedConnection);
    connect(remover, &FileRemover::sig_finished,
            remove_dialog_, &FileRemoveDialog::stop,
            Qt::QueuedConnection);
    connect(remover, &FileRemover::sig_errorOccurred,
            remove_dialog_, &FileRemoveDialog::errorOccurred,
            Qt::QueuedConnection);
    connect(remover, &FileRemover::sig_progressChanged,
            remove_dialog_, &FileRemoveDialog::setCurrentProgress,
            Qt::QueuedConnection);
    connect(remove_dialog_, &FileRemoveDialog::sig_action,
            remover, &FileRemover::setAction,
            Qt::QueuedConnection);
    connect(remove_dialog_, &FileRemoveDialog::sig_stop,
            remover, &FileRemover::stop,
            Qt::QueuedConnection);

    emit sig_removeRequest(remover);
}

//--------------------------------------------------------------------------------------------------
void FileTransferSessionWindow::sendItems(
    FilePanel* sender, const QList<FileTransfer::Item>& items)
{
    if (sender == ui->local_panel)
    {
        transferItems(FileTransfer::Type::UPLOADER,
                      ui->local_panel->currentPath(),
                      ui->remote_panel->currentPath(),
                      items);
    }
    else
    {
        DCHECK(sender == ui->remote_panel);

        transferItems(FileTransfer::Type::DOWNLOADER,
                      ui->remote_panel->currentPath(),
                      ui->local_panel->currentPath(),
                      items);
    }
}

//--------------------------------------------------------------------------------------------------
void FileTransferSessionWindow::receiveItems(
    FilePanel* sender, const QString& target_folder, const QList<FileTransfer::Item>& items)
{
    if (sender == ui->local_panel)
    {
        transferItems(FileTransfer::Type::DOWNLOADER,
                      ui->remote_panel->currentPath(),
                      target_folder,
                      items);
    }
    else
    {
        DCHECK(sender == ui->remote_panel);

        transferItems(FileTransfer::Type::UPLOADER,
                      ui->local_panel->currentPath(),
                      target_folder,
                      items);
    }
}

//--------------------------------------------------------------------------------------------------
void FileTransferSessionWindow::transferItems(
    FileTransfer::Type type, const QString& source_path, const QString& target_path,
    const QList<FileTransfer::Item>& items)
{
    transfer_dialog_ = new FileTransferDialog(this);
    transfer_dialog_->setAttribute(Qt::WA_DeleteOnClose);

    connect(transfer_dialog_, &FileRemoveDialog::finished, this, [this]()
    {
        refresh();
        activateWindow();
        setFocus();

        ui->local_panel->setEnabled(true);
        ui->remote_panel->setEnabled(true);
    });

    ui->local_panel->setEnabled(false);
    ui->remote_panel->setEnabled(false);

    FileTransfer* transfer = new FileTransfer(type, source_path, target_path, items);
    transfer->moveToThread(base::GuiApplication::ioThread());

    connect(transfer, &FileTransfer::sig_started,
            transfer_dialog_, &FileTransferDialog::start,
            Qt::QueuedConnection);
    connect(transfer, &FileTransfer::sig_finished,
            transfer_dialog_, &FileTransferDialog::stop,
            Qt::QueuedConnection);
    connect(transfer, &FileTransfer::sig_errorOccurred,
            transfer_dialog_, &FileTransferDialog::errorOccurred,
            Qt::QueuedConnection);
    connect(transfer, &FileTransfer::sig_progressChanged,
            transfer_dialog_, &FileTransferDialog::setCurrentProgress,
            Qt::QueuedConnection);
    connect(transfer, &FileTransfer::sig_currentItemChanged,
            transfer_dialog_, &FileTransferDialog::setCurrentItem,
            Qt::QueuedConnection);
    connect(transfer, &FileTransfer::sig_currentSpeedChanged,
            transfer_dialog_, &FileTransferDialog::setCurrentSpeed,
            Qt::QueuedConnection);

    connect(transfer_dialog_, &FileTransferDialog::sig_action,
            transfer, &FileTransfer::setAction,
            Qt::QueuedConnection);
    connect(transfer_dialog_, &FileTransferDialog::sig_stop,
            transfer, &FileTransfer::stop,
            Qt::QueuedConnection);

    emit sig_transferRequest(transfer);
}

//--------------------------------------------------------------------------------------------------
void FileTransferSessionWindow::onPathChanged(FilePanel* sender, const QString& path)
{
    bool allow = path != AddressBarModel::computerPath();

    if (sender == ui->local_panel)
    {
        ui->remote_panel->setTransferAllowed(allow);
    }
    else
    {
        DCHECK(sender == ui->remote_panel);
        ui->local_panel->setTransferAllowed(allow);

    }
}

//--------------------------------------------------------------------------------------------------
void FileTransferSessionWindow::initPanel(
    common::FileTask::Target target, const QString& title, const QString& mime_type, FilePanel* panel)
{
    LOG(LS_INFO) << "Init file manager panel (target=" << static_cast<int>(target) << ")";

    panel->setPanelName(title);
    panel->setMimeType(mime_type);

    connect(panel, &FilePanel::sig_driveList, this, [this, target]()
    {
        emit sig_driveListRequest(target);
    });

    connect(panel, &FilePanel::sig_fileList, this, [this, target](const QString& path)
    {
        emit sig_fileListRequest(target, path);
    });

    connect(panel, &FilePanel::sig_rename,
            this, [this, target](const QString& old_path, const QString& new_path)
    {
        emit sig_renameRequest(target, old_path, new_path);
    });

    connect(panel, &FilePanel::sig_createDirectory, this, [this, target](const QString& path)
    {
        emit sig_createDirectoryRequest(target, path);
    });

    connect(panel, &FilePanel::sig_removeItems, this, &FileTransferSessionWindow::removeItems);
    connect(panel, &FilePanel::sig_sendItems, this, &FileTransferSessionWindow::sendItems);
    connect(panel, &FilePanel::sig_receiveItems, this, &FileTransferSessionWindow::receiveItems);
    connect(panel, &FilePanel::sig_pathChanged, this, &FileTransferSessionWindow::onPathChanged);
}

} // namespace client
