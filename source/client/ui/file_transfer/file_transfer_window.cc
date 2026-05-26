//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "client/ui/file_transfer/file_transfer_window.h"

#include "base/gui_application.h"
#include "base/logging.h"
#include "client/client_file_transfer.h"
#include "client/ui/file_transfer/address_bar_model.h"
#include "client/ui/file_transfer/file_error_code.h"
#include "client/ui/file_transfer/file_mime_data.h"
#include "client/ui/file_transfer/file_remove_widget.h"
#include "client/ui/file_transfer/file_transfer_widget.h"

#include "common/ui/msg_box.h"
#include "common/ui/session_type.h"
#include "proto/peer.h"
#include "ui_file_transfer_window.h"

//--------------------------------------------------------------------------------------------------
FileTransferWindow::FileTransferWindow(QWidget* parent)
    : ClientWindow(proto::peer::SESSION_TYPE_FILE_TRANSFER, parent),
      ui(std::make_unique<Ui::FileTransferWindow>())
{
    LOG(INFO) << "Ctor";

    ui->setupUi(this);

    QString mime_type = FileMimeData::createMimeType();

    initPanel(FileTask::Target::LOCAL, tr("Local Computer"), mime_type, ui->local_panel);
    initPanel(FileTask::Target::REMOTE, tr("Remote Computer"), mime_type, ui->remote_panel);

    connect(ui->transfer_widget, &FileTransferWidget::sig_finished,
            this, &FileTransferWindow::onTransferWidgetFinished);
    connect(ui->remove_widget, &FileRemoveWidget::sig_finished,
            this, &FileTransferWindow::onRemoveWidgetFinished);

    auto make_action = [this](proto::peer::SessionType type)
    {
        QAction* action = new QAction(sessionIcon(type), sessionName(type), this);
        connect(action, &QAction::triggered, this, [this, type]()
        {
            emit sig_connectRequested(sessionState()->host(), type);
        });
        return action;
    };

    action_desktop_ = make_action(proto::peer::SESSION_TYPE_DESKTOP);
    action_text_chat_ = make_action(proto::peer::SESSION_TYPE_CHAT);
    action_system_info_ = make_action(proto::peer::SESSION_TYPE_SYSTEM_INFO);

    ui->local_panel->setFocus();
}

//--------------------------------------------------------------------------------------------------
FileTransferWindow::~FileTransferWindow()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
Client* FileTransferWindow::createClient()
{
    LOG(INFO) << "Create client";

    ClientFileTransfer* client = new ClientFileTransfer();

    connect(client, &ClientFileTransfer::sig_showSessionWindow, this, &FileTransferWindow::onShowWindow,
            Qt::QueuedConnection);
    connect(client, &ClientFileTransfer::sig_errorOccurred, this, &FileTransferWindow::onErrorOccurred,
            Qt::QueuedConnection);
    connect(client, &ClientFileTransfer::sig_driveListReply, this, &FileTransferWindow::onDriveList,
            Qt::QueuedConnection);
    connect(client, &ClientFileTransfer::sig_fileListReply, this, &FileTransferWindow::onFileList,
            Qt::QueuedConnection);
    connect(client, &ClientFileTransfer::sig_createDirectoryReply, this, &FileTransferWindow::onCreateDirectory,
            Qt::QueuedConnection);
    connect(client, &ClientFileTransfer::sig_renameReply, this, &FileTransferWindow::onRename,
            Qt::QueuedConnection);

    connect(this, &FileTransferWindow::sig_driveListRequest, client, &ClientFileTransfer::onDriveListRequest,
            Qt::QueuedConnection);
    connect(this, &FileTransferWindow::sig_fileListRequest, client, &ClientFileTransfer::onFileListRequest,
            Qt::QueuedConnection);
    connect(this, &FileTransferWindow::sig_createDirectoryRequest, client, &ClientFileTransfer::onCreateDirectoryRequest,
            Qt::QueuedConnection);
    connect(this, &FileTransferWindow::sig_renameRequest, client, &ClientFileTransfer::onRenameRequest,
            Qt::QueuedConnection);
    connect(this, &FileTransferWindow::sig_removeRequest, client, &ClientFileTransfer::onRemoveRequest,
            Qt::QueuedConnection);
    connect(this, &FileTransferWindow::sig_transferRequest, client, &ClientFileTransfer::onTransferRequest,
            Qt::QueuedConnection);

    return client;
}

//--------------------------------------------------------------------------------------------------
QList<QPair<Tab::ActionRole, QList<QAction*>>> FileTransferWindow::tabActionGroups() const
{
    return {{ Tab::ActionRole::ACTION, { action_desktop_, action_text_chat_, action_system_info_ }}};
}

//--------------------------------------------------------------------------------------------------
QByteArray FileTransferWindow::saveState() const
{
    QByteArray buffer;

    {
        QDataStream stream(&buffer, QIODevice::WriteOnly);
        stream.setVersion(QDataStream::Qt_6_10);
        stream << saveGeometry();
        stream << ui->splitter->saveState();
        stream << ui->local_panel->saveState();
        stream << ui->remote_panel->saveState();
    }

    return buffer;
}

//--------------------------------------------------------------------------------------------------
void FileTransferWindow::restoreState(const QByteArray& state)
{
    QDataStream stream(state);
    stream.setVersion(QDataStream::Qt_6_10);

    QByteArray value;

    stream >> value;
    restoreGeometry(value);

    stream >> value;
    ui->splitter->restoreState(value);

    stream >> value;
    ui->local_panel->restoreState(value);

    stream >> value;
    ui->remote_panel->restoreState(value);
}

//--------------------------------------------------------------------------------------------------
void FileTransferWindow::onShowWindow()
{
    LOG(INFO) << "Show window";
    show();
    activateWindow();
    refresh();
}

//--------------------------------------------------------------------------------------------------
void FileTransferWindow::onErrorOccurred(proto::file_transfer::ErrorCode error_code)
{
    LOG(ERROR) << "Session error:" << error_code;
    MsgBox::warning(this,
                                tr("Session error: %1").arg(fileErrorToString(error_code)));
    close();
}

//--------------------------------------------------------------------------------------------------
void FileTransferWindow::onDriveList(
    FileTask::Target target, proto::file_transfer::ErrorCode error_code,
    const proto::file_transfer::DriveList& drive_list)
{
    if (target == FileTask::Target::LOCAL)
    {
        ui->local_panel->onDriveList(error_code, drive_list);
    }
    else
    {
        DCHECK_EQ(target, FileTask::Target::REMOTE);
        ui->remote_panel->onDriveList(error_code, drive_list);
    }
}

//--------------------------------------------------------------------------------------------------
void FileTransferWindow::onFileList(
    FileTask::Target target, proto::file_transfer::ErrorCode error_code,
    const proto::file_transfer::List& file_list)
{
    if (target == FileTask::Target::LOCAL)
    {
        ui->local_panel->onFileList(error_code, file_list);
    }
    else
    {
        DCHECK_EQ(target, FileTask::Target::REMOTE);
        ui->remote_panel->onFileList(error_code, file_list);
    }
}

//--------------------------------------------------------------------------------------------------
void FileTransferWindow::onCreateDirectory(
    FileTask::Target target, proto::file_transfer::ErrorCode error_code)
{
    if (target == FileTask::Target::LOCAL)
    {
        ui->local_panel->onCreateDirectory(error_code);
    }
    else
    {
        DCHECK_EQ(target, FileTask::Target::REMOTE);
        ui->remote_panel->onCreateDirectory(error_code);
    }
}

//--------------------------------------------------------------------------------------------------
void FileTransferWindow::onRename(
    FileTask::Target target, proto::file_transfer::ErrorCode error_code)
{
    if (target == FileTask::Target::LOCAL)
    {
        ui->local_panel->onRename(error_code);
    }
    else
    {
        DCHECK_EQ(target, FileTask::Target::REMOTE);
        ui->remote_panel->onRename(error_code);
    }
}

//--------------------------------------------------------------------------------------------------
void FileTransferWindow::refresh()
{
    ui->local_panel->refresh();
    ui->remote_panel->refresh();
}

//--------------------------------------------------------------------------------------------------
void FileTransferWindow::onInternalReset()
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
void FileTransferWindow::changeEvent(QEvent* event)
{
    ClientWindow::changeEvent(event);

    if (event->type() == QEvent::LanguageChange)
    {
        ui->retranslateUi(this);
        ui->local_panel->setPanelName(tr("Local Computer"));
        ui->remote_panel->setPanelName(tr("Remote Computer"));
    }
}

//--------------------------------------------------------------------------------------------------
void FileTransferWindow::closeEvent(QCloseEvent* event)
{
    LOG(INFO) << "Close event detected";

    if (ui->transfer_widget->isVisible())
    {
        LOG(INFO) << "Stopping transfer widget";
        ui->transfer_widget->requestStop();
    }

    if (ui->remove_widget->isVisible())
    {
        LOG(INFO) << "Stopping remove widget";
        ui->remove_widget->requestStop();
    }

    ClientWindow::closeEvent(event);
}

//--------------------------------------------------------------------------------------------------
void FileTransferWindow::removeItems(FilePanel* sender, const FileRemover::TaskList& items)
{
    FileTask::Target target;

    if (sender == ui->local_panel)
    {
        target = FileTask::Target::LOCAL;
    }
    else
    {
        DCHECK_EQ(sender, ui->remote_panel);
        target = FileTask::Target::REMOTE;
    }

    ui->remove_widget->reset();
    ui->remove_widget->setVisible(true);

    setFilePanelsEnabled(false);

    FileRemover* remover = new FileRemover(target, items);
    remover->moveToThread(GuiApplication::ioThread());

    connect(remover, &FileRemover::sig_started, ui->remove_widget, &FileRemoveWidget::start,
            Qt::QueuedConnection);
    connect(remover, &FileRemover::sig_finished, ui->remove_widget, &FileRemoveWidget::stop,
            Qt::QueuedConnection);
    connect(remover, &FileRemover::sig_errorOccurred, ui->remove_widget, &FileRemoveWidget::errorOccurred,
            Qt::QueuedConnection);
    connect(remover, &FileRemover::sig_progressChanged, ui->remove_widget, &FileRemoveWidget::setCurrentProgress,
            Qt::QueuedConnection);
    connect(ui->remove_widget, &FileRemoveWidget::sig_action, remover, &FileRemover::setAction,
            Qt::QueuedConnection);
    connect(ui->remove_widget, &FileRemoveWidget::sig_stop, remover, &FileRemover::stop,
            Qt::QueuedConnection);

    emit sig_removeRequest(remover);
}

//--------------------------------------------------------------------------------------------------
void FileTransferWindow::sendItems(
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
void FileTransferWindow::receiveItems(
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
void FileTransferWindow::onPathChanged(FilePanel* sender, const QString& path)
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
void FileTransferWindow::onTransferWidgetFinished()
{
    ui->transfer_widget->setVisible(false);
    setFilePanelsEnabled(true);
    refresh();
    activateWindow();
    setFocus();
}

//--------------------------------------------------------------------------------------------------
void FileTransferWindow::onRemoveWidgetFinished()
{
    ui->remove_widget->setVisible(false);
    setFilePanelsEnabled(true);
    refresh();
    activateWindow();
    setFocus();
}

//--------------------------------------------------------------------------------------------------
void FileTransferWindow::transferItems(
    FileTransfer::Type type, const QString& source_path, const QString& target_path,
    const QList<FileTransfer::Item>& items)
{
    ui->transfer_widget->reset();
    ui->transfer_widget->setVisible(true);

    setFilePanelsEnabled(false);

    FileTransfer* transfer = new FileTransfer(type, source_path, target_path, items);
    transfer->moveToThread(GuiApplication::ioThread());

    connect(transfer, &FileTransfer::sig_started, ui->transfer_widget, &FileTransferWidget::start,
            Qt::QueuedConnection);
    connect(transfer, &FileTransfer::sig_finished, ui->transfer_widget, &FileTransferWidget::stop,
            Qt::QueuedConnection);
    connect(transfer, &FileTransfer::sig_errorOccurred, ui->transfer_widget, &FileTransferWidget::errorOccurred,
            Qt::QueuedConnection);
    connect(transfer, &FileTransfer::sig_progressChanged, ui->transfer_widget, &FileTransferWidget::setCurrentProgress,
            Qt::QueuedConnection);
    connect(transfer, &FileTransfer::sig_currentItemChanged, ui->transfer_widget, &FileTransferWidget::setCurrentItem,
            Qt::QueuedConnection);
    connect(transfer, &FileTransfer::sig_currentSpeedChanged, ui->transfer_widget, &FileTransferWidget::setCurrentSpeed,
            Qt::QueuedConnection);

    connect(ui->transfer_widget, &FileTransferWidget::sig_action, transfer, &FileTransfer::setAction,
            Qt::QueuedConnection);
    connect(ui->transfer_widget, &FileTransferWidget::sig_stop, transfer, &FileTransfer::stop,
            Qt::QueuedConnection);

    emit sig_transferRequest(transfer);
}

//--------------------------------------------------------------------------------------------------
void FileTransferWindow::initPanel(
    FileTask::Target target, const QString& title, const QString& mime_type, FilePanel* panel)
{
    LOG(INFO) << "Init file manager panel (target=" << static_cast<int>(target) << ")";

    panel->setPanelName(title);
    panel->setMimeType(mime_type);
    panel->setMirrored(target == FileTask::Target::REMOTE);

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

    connect(panel, &FilePanel::sig_removeItems, this, &FileTransferWindow::removeItems);
    connect(panel, &FilePanel::sig_sendItems, this, &FileTransferWindow::sendItems);
    connect(panel, &FilePanel::sig_receiveItems, this, &FileTransferWindow::receiveItems);
    connect(panel, &FilePanel::sig_pathChanged, this, &FileTransferWindow::onPathChanged);
}

//--------------------------------------------------------------------------------------------------
void FileTransferWindow::setFilePanelsEnabled(bool enabled)
{
    ui->local_panel->setEnabled(enabled);
    ui->remote_panel->setEnabled(enabled);
}
