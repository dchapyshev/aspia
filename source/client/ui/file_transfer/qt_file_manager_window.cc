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

#include "client/ui/file_transfer/qt_file_manager_window.h"

#include "ui_qt_file_manager_window.h"
#include "base/logging.h"
#include "client/file_control_proxy.h"
#include "client/file_manager_window_proxy.h"
#include "client/client_file_transfer.h"
#include "client/ui/file_transfer/address_bar_model.h"
#include "client/ui/file_transfer/file_error_code.h"
#include "client/ui/file_transfer/file_remove_dialog.h"
#include "client/ui/file_transfer/file_transfer_dialog.h"
#include "client/ui/file_transfer/file_manager_settings.h"
#include "client/ui/file_transfer/file_mime_data.h"
#include "qt_base/application.h"

#include <QMessageBox>

namespace client {

//--------------------------------------------------------------------------------------------------
QtFileManagerWindow::QtFileManagerWindow(QWidget* parent)
    : SessionWindow(nullptr, parent),
      ui(std::make_unique<Ui::FileManagerWindow>()),
      file_manager_window_proxy_(
          std::make_shared<FileManagerWindowProxy>(base::GuiApplication::uiTaskRunner(), this))
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
QtFileManagerWindow::~QtFileManagerWindow()
{
    LOG(LS_INFO) << "Dtor";
    file_manager_window_proxy_->dettach();
}

//--------------------------------------------------------------------------------------------------
std::unique_ptr<Client> QtFileManagerWindow::createClient()
{
    LOG(LS_INFO) << "Create client";

    std::unique_ptr<ClientFileTransfer> client = std::make_unique<ClientFileTransfer>(
        base::GuiApplication::ioTaskRunner());

    client->setFileManagerWindow(file_manager_window_proxy_);

    return std::move(client);
}

//--------------------------------------------------------------------------------------------------
void QtFileManagerWindow::start(std::shared_ptr<FileControlProxy> file_control_proxy)
{
    LOG(LS_INFO) << "Show window";

    file_control_proxy_ = std::move(file_control_proxy);
    DCHECK(file_control_proxy_);

    show();
    activateWindow();
    refresh();
}

//--------------------------------------------------------------------------------------------------
void QtFileManagerWindow::onErrorOccurred(proto::FileError error_code)
{
    LOG(LS_ERROR) << "Session error: " << error_code;
    QMessageBox::warning(this,
                         tr("Warning"),
                         tr("Session error: %1").arg(fileErrorToString(error_code)),
                         QMessageBox::Ok);
    close();
}

//--------------------------------------------------------------------------------------------------
void QtFileManagerWindow::onDriveList(
    common::FileTask::Target target, proto::FileError error_code, const proto::DriveList& drive_list)
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
void QtFileManagerWindow::onFileList(
    common::FileTask::Target target, proto::FileError error_code, const proto::FileList& file_list)
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
void QtFileManagerWindow::onCreateDirectory(
    common::FileTask::Target target, proto::FileError error_code)
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
void QtFileManagerWindow::onRename(common::FileTask::Target target, proto::FileError error_code)
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
QByteArray QtFileManagerWindow::saveState() const
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
void QtFileManagerWindow::restoreState(const QByteArray& state)
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
void QtFileManagerWindow::refresh()
{
    ui->local_panel->refresh();
    ui->remote_panel->refresh();
}

//--------------------------------------------------------------------------------------------------
void QtFileManagerWindow::onInternalReset()
{
    // TODO
}

//--------------------------------------------------------------------------------------------------
void QtFileManagerWindow::closeEvent(QCloseEvent* event)
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
void QtFileManagerWindow::removeItems(FilePanel* sender, const FileRemover::TaskList& items)
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

    file_control_proxy_->remove(target, remove_dialog_->windowProxy(), items);
}

//--------------------------------------------------------------------------------------------------
void QtFileManagerWindow::sendItems(FilePanel* sender, const std::vector<FileTransfer::Item>& items)
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
void QtFileManagerWindow::receiveItems(FilePanel* sender,
                                       const QString& target_folder,
                                       const std::vector<FileTransfer::Item>& items)
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
void QtFileManagerWindow::transferItems(FileTransfer::Type type,
                                        const QString& source_path,
                                        const QString& target_path,
                                        const std::vector<FileTransfer::Item>& items)
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

    file_control_proxy_->transfer(
        transfer_dialog_->windowProxy(),
        type,
        source_path.toStdString(),
        target_path.toStdString(),
        items);
}

//--------------------------------------------------------------------------------------------------
void QtFileManagerWindow::onPathChanged(FilePanel* sender, const QString& path)
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
void QtFileManagerWindow::initPanel(
    common::FileTask::Target target, const QString& title, const QString& mime_type, FilePanel* panel)
{
    LOG(LS_INFO) << "Init file manager panel (target=" << static_cast<int>(target) << ")";

    panel->setPanelName(title);
    panel->setMimeType(mime_type);

    connect(panel, &FilePanel::sig_driveList, this, [this, target]()
    {
        file_control_proxy_->driveList(target);
    });

    connect(panel, &FilePanel::sig_fileList, this, [this, target](const QString& path)
    {
        file_control_proxy_->fileList(target, path.toStdString());
    });

    connect(panel, &FilePanel::sig_rename,
            this, [this, target](const QString& old_path, const QString& new_path)
    {
        file_control_proxy_->rename(target, old_path.toStdString(), new_path.toStdString());
    });

    connect(panel, &FilePanel::sig_createDirectory, this, [this, target](const QString& path)
    {
        file_control_proxy_->createDirectory(target, path.toStdString());
    });

    connect(panel, &FilePanel::sig_removeItems, this, &QtFileManagerWindow::removeItems);
    connect(panel, &FilePanel::sig_sendItems, this, &QtFileManagerWindow::sendItems);
    connect(panel, &FilePanel::sig_receiveItems, this, &QtFileManagerWindow::receiveItems);
    connect(panel, &FilePanel::sig_pathChanged, this, &QtFileManagerWindow::onPathChanged);
}

} // namespace client
