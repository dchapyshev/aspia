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

#include "client/android/file_transfer_window.h"

#include <QHBoxLayout>
#include <QResizeEvent>
#include <QVBoxLayout>

#include "base/gui_application.h"
#include "client/client_file_transfer.h"
#include "client/database.h"
#include "client/router.h"
#include "client/session_state.h"
#include "client/android/file_panel_widget.h"
#include "client/android/file_progress_sheet.h"
#include "common/android/app_bar.h"
#include "common/android/label.h"
#include "common/android/message_dialog.h"
#include "common/android/tab_bar.h"
#include "proto/peer.h"
#include "proto/router_client.h"

#if defined(Q_OS_ANDROID)
#include <QCoreApplication>
#include <QJniObject>
#endif // defined(Q_OS_ANDROID)

//--------------------------------------------------------------------------------------------------
FileTransferWindow::FileTransferWindow(const HostConfig& host, QWidget* parent)
    : QWidget(parent),
      host_(host),
      app_bar_(new AppBar(this)),
      status_(new Label(QString(), Label::Role::CAPTION, this)),
      local_panel_(new FilePanelWidget(FileTask::Target::LOCAL, this)),
      remote_panel_(new FilePanelWidget(FileTask::Target::REMOTE, this))
{
    const QString host_title = host_.name().isEmpty() ? host_.address() : host_.name();

    app_bar_->setTitle(tr("File Transfer"));
    app_bar_->setBackVisible(true);
    app_bar_->setBottomBorderVisible(false);
    connect(app_bar_, &AppBar::sig_backClicked, this, &FileTransferWindow::sig_closed);

    local_panel_->setTitle(tr("This Device"));
    remote_panel_->setTitle(host_title);

    // Ignored horizontal policy drops the panels' minimum width from the layout calculation. Without
    // it the two panels' minimums add up and stretch the top-level window wider than the screen (the
    // right panel/tab then renders off-screen). The panel contents scale down fine.
    local_panel_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    remote_panel_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);

    status_->setAlignment(Qt::AlignCenter);

    // Switcher shown only on a narrow screen.
    tab_bar_ = new TabBar(this);
    tab_bar_->addTab(tr("This Device"));
    tab_bar_->addTab(host_title);
    connect(tab_bar_, &TabBar::sig_currentChanged, this, &FileTransferWindow::setActivePanel);

    QWidget* panels = new QWidget(this);
    panels_layout_ = new QHBoxLayout(panels);
    panels_layout_->setContentsMargins(0, 0, 0, 0);
    panels_layout_->addWidget(local_panel_, 1);
    panels_layout_->addWidget(remote_panel_, 1);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(app_bar_);
    layout->addWidget(status_);
    layout->addWidget(panels, 1);
    layout->addWidget(tab_bar_);

    initPanel(local_panel_, FileTask::Target::LOCAL);
    initPanel(remote_panel_, FileTask::Target::REMOTE);

    applyLayout();
    start();

    // Deferred so the prompt's dialog has a visible parent once the window is on screen.
    QMetaObject::invokeMethod(this, &FileTransferWindow::ensureStoragePermission, Qt::QueuedConnection);
}

//--------------------------------------------------------------------------------------------------
void FileTransferWindow::ensureStoragePermission()
{
#if defined(Q_OS_ANDROID)
    if (QJniObject::callStaticMethod<jboolean>(
            "android/os/Environment", "isExternalStorageManager", "()Z"))
    {
        return;
    }

    if (!MessageDialog::confirm(this, tr("File Transfer"),
            tr("To browse files on this device, allow access to all files on the next screen."),
            tr("Allow")))
    {
        return;
    }

    QJniObject context = QNativeInterface::QAndroidApplication::context();
    if (!context.isValid())
        return;

    const QString package = context.callObjectMethod(
        "getPackageName", "()Ljava/lang/String;").toString();
    QJniObject uri = QJniObject::callStaticObjectMethod(
        "android/net/Uri", "parse", "(Ljava/lang/String;)Landroid/net/Uri;",
        QJniObject::fromString("package:" + package).object<jstring>());

    QJniObject intent("android/content/Intent", "(Ljava/lang/String;Landroid/net/Uri;)V",
        QJniObject::fromString("android.settings.MANAGE_APP_ALL_FILES_ACCESS_PERMISSION").object<jstring>(),
        uri.object());

    // FLAG_ACTIVITY_NEW_TASK, required to start an activity from a non-activity context.
    intent.callObjectMethod("addFlags", "(I)Landroid/content/Intent;", 0x10000000);
    context.callMethod<void>("startActivity", "(Landroid/content/Intent;)V", intent.object());
#endif // defined(Q_OS_ANDROID)
}

//--------------------------------------------------------------------------------------------------
FileTransferWindow::~FileTransferWindow() = default;

//--------------------------------------------------------------------------------------------------
void FileTransferWindow::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    applyLayout();
}

//--------------------------------------------------------------------------------------------------
void FileTransferWindow::onStatusChanged(Client::Status status, const QVariant& /* data */)
{
    switch (status)
    {
        case Client::Status::HOST_CONNECTING:
            setStatusText(tr("Connecting to host %1...").arg(session_state_->hostAddress()));
            break;

        case Client::Status::HOST_CONNECTED:
            connected_ = true;
            status_->setVisible(false);
            local_panel_->refresh();
            remote_panel_->refresh();
            break;

        case Client::Status::HOST_DISCONNECTED:
            setStatusText(tr("The connection to the host has been lost."));
            break;

        case Client::Status::NO_ROUTER:
            setStatusText(tr("The specified router is unavailable."));
            break;

        case Client::Status::ROUTER_OFFLINE:
            setStatusText(tr("The specified router is offline."));
            break;

        case Client::Status::ROUTER_ERROR:
            setStatusText(tr("Error requesting connection via router."));
            break;

        case Client::Status::VERSION_MISMATCH:
            setStatusText(tr("The host version is newer than the client. Please update the application."));
            break;

        default:
            break;
    }
}

//--------------------------------------------------------------------------------------------------
void FileTransferWindow::onErrorOccurred(proto::file_transfer::ErrorCode /* error_code */)
{
    MessageDialog::info(this, tr("File Transfer"),
                        tr("There is no logged in user on the host. The session is unavailable."));
    emit sig_closed();
}

//--------------------------------------------------------------------------------------------------
void FileTransferWindow::onDriveList(
    FileTask::Target target, proto::file_transfer::ErrorCode error_code,
    const proto::file_transfer::DriveList& drive_list)
{
    if (target == FileTask::Target::LOCAL)
        local_panel_->onDriveList(error_code, drive_list);
    else
        remote_panel_->onDriveList(error_code, drive_list);
}

//--------------------------------------------------------------------------------------------------
void FileTransferWindow::onFileList(
    FileTask::Target target, proto::file_transfer::ErrorCode error_code,
    const proto::file_transfer::List& file_list)
{
    if (target == FileTask::Target::LOCAL)
        local_panel_->onFileList(error_code, file_list);
    else
        remote_panel_->onFileList(error_code, file_list);
}

//--------------------------------------------------------------------------------------------------
void FileTransferWindow::onCreateDirectory(
    FileTask::Target target, proto::file_transfer::ErrorCode error_code)
{
    if (target == FileTask::Target::LOCAL)
        local_panel_->onCreateDirectory(error_code);
    else
        remote_panel_->onCreateDirectory(error_code);
}

//--------------------------------------------------------------------------------------------------
void FileTransferWindow::start()
{
    // An empty user name means a connection by ID with a one-time password (#host_id).
    if (host_.username().isEmpty())
        host_.setUsername(u"#" + host_.address());

    session_state_ = std::make_shared<SessionState>(
        host_, proto::peer::SESSION_TYPE_FILE_TRANSFER, Database::instance().displayName());

    setStatusText(tr("Connecting..."));

    if (session_state_->isConnectionByHostId())
        fetchConnectionOffer();
    else
        startNewClient();
}

//--------------------------------------------------------------------------------------------------
void FileTransferWindow::fetchConnectionOffer()
{
    Router* router = Router::instance(session_state_->routerId());
    if (!router)
    {
        setStatusText(tr("The specified router is unavailable."));
        return;
    }

    if (router->status() == Router::Status::ONLINE)
    {
        requestConnectionOffer(router);
        return;
    }

    // The router connection is also dropped while the app is in the background.
    setStatusText(tr("Connecting to router..."));

    disconnect(router, &Router::sig_statusChanged, this, nullptr);
    connect(router, &Router::sig_statusChanged, this,
        [this](qint64 /* router_id */, Router::Status status)
    {
        if (status != Router::Status::ONLINE)
            return;

        Router* router = Router::instance(session_state_->routerId());
        if (!router)
            return;

        disconnect(router, &Router::sig_statusChanged, this, nullptr);
        requestConnectionOffer(router);
    });

    if (router->status() == Router::Status::OFFLINE)
        router->connectToRouter();
}

//--------------------------------------------------------------------------------------------------
void FileTransferWindow::requestConnectionOffer(Router* router)
{
    session_state_->setRouterVersion(router->version());
    setStatusText(tr("Requesting connection to the host..."));

    router->requestConnection(session_state_->hostId(), this,
        [this](const proto::router::ConnectionOffer& offer)
    {
        if (offer.error_code() == proto::router::ConnectionOffer::SUCCESS)
        {
            session_state_->setConnectionOffer(offer);
            startNewClient();
            return;
        }

        setStatusText(tr("Error requesting connection via router."));
    });
}

//--------------------------------------------------------------------------------------------------
void FileTransferWindow::startNewClient()
{
    ClientFileTransfer* client = new ClientFileTransfer();

    connect(client, &Client::sig_statusChanged,
            this, &FileTransferWindow::onStatusChanged, Qt::QueuedConnection);
    connect(client, &ClientFileTransfer::sig_errorOccurred,
            this, &FileTransferWindow::onErrorOccurred, Qt::QueuedConnection);
    connect(client, &ClientFileTransfer::sig_driveListReply,
            this, &FileTransferWindow::onDriveList, Qt::QueuedConnection);
    connect(client, &ClientFileTransfer::sig_fileListReply,
            this, &FileTransferWindow::onFileList, Qt::QueuedConnection);
    connect(client, &ClientFileTransfer::sig_createDirectoryReply,
            this, &FileTransferWindow::onCreateDirectory, Qt::QueuedConnection);

    connect(this, &FileTransferWindow::sig_driveListRequest,
            client, &ClientFileTransfer::onDriveListRequest, Qt::QueuedConnection);
    connect(this, &FileTransferWindow::sig_fileListRequest,
            client, &ClientFileTransfer::onFileListRequest, Qt::QueuedConnection);
    connect(this, &FileTransferWindow::sig_createDirectoryRequest,
            client, &ClientFileTransfer::onCreateDirectoryRequest, Qt::QueuedConnection);
    connect(this, &FileTransferWindow::sig_removeRequest,
            client, &ClientFileTransfer::onRemoveRequest, Qt::QueuedConnection);
    connect(this, &FileTransferWindow::sig_transferRequest,
            client, &ClientFileTransfer::onTransferRequest, Qt::QueuedConnection);

    client->moveToThread(GuiApplication::ioThread());
    client->setSessionState(session_state_);
    client_ = client;

    QMetaObject::invokeMethod(client, &Client::start, Qt::QueuedConnection);
}

//--------------------------------------------------------------------------------------------------
void FileTransferWindow::initPanel(FilePanelWidget* panel, FileTask::Target target)
{
    connect(panel, &FilePanelWidget::sig_driveListRequested, this, [this, target]()
    {
        emit sig_driveListRequest(target);
    });
    connect(panel, &FilePanelWidget::sig_fileListRequested, this, [this, target](const QString& path)
    {
        emit sig_fileListRequest(target, path);
    });
    connect(panel, &FilePanelWidget::sig_createDirectoryRequested, this, [this, target](const QString& path)
    {
        emit sig_createDirectoryRequest(target, path);
    });
    connect(panel, &FilePanelWidget::sig_sendRequested, this, [this, panel](const QList<FileTransfer::Item>& items)
    {
        sendItems(panel, items);
    });
    connect(panel, &FilePanelWidget::sig_removeRequested, this, [this, panel](const FileRemover::TaskList& items)
    {
        removeItems(panel, items);
    });
    connect(panel, &FilePanelWidget::sig_pathChanged, this, [this, panel](const QString& path)
    {
        // A folder open on one side is the destination the other side can send into.
        FilePanelWidget* other = (panel == local_panel_) ? remote_panel_ : local_panel_;
        other->setTransferAllowed(!path.isEmpty());
    });
}

//--------------------------------------------------------------------------------------------------
void FileTransferWindow::sendItems(FilePanelWidget* sender, const QList<FileTransfer::Item>& items)
{
    FilePanelWidget* other = (sender == local_panel_) ? remote_panel_ : local_panel_;
    const QString target_path = other->currentPath();

    if (target_path.isEmpty())
    {
        MessageDialog::info(this, tr("File Transfer"),
                            tr("Open a destination folder on the other side first."));
        return;
    }

    const FileTransfer::Type type = (sender == local_panel_) ?
        FileTransfer::Type::UPLOADER : FileTransfer::Type::DOWNLOADER;

    FileTransfer* transfer = new FileTransfer(type, sender->currentPath(), target_path, items);
    transfer->moveToThread(GuiApplication::ioThread());

    FileProgressSheet* progress = new FileProgressSheet(tr("File Transfer"), this);
    progress->setAttribute(Qt::WA_DeleteOnClose);

    connect(transfer, &FileTransfer::sig_progressChanged, progress,
            [progress](int total, int /* current */) { progress->setProgress(total); }, Qt::QueuedConnection);
    connect(transfer, &FileTransfer::sig_currentItemChanged, progress,
            [progress](const QString& source, const QString& /* target */)
    {
        progress->setCurrentItem(source);
    }, Qt::QueuedConnection);
    connect(transfer, &FileTransfer::sig_currentSpeedChanged, progress,
            &FileProgressSheet::setSpeed, Qt::QueuedConnection);
    connect(transfer, &FileTransfer::sig_errorOccurred, this, [transfer](const FileTransfer::Error& error)
    {
        // MVP: overwrite on conflict, skip other errors, to keep the queue moving without prompting.
        const FileTransfer::Error::Action action =
            (error.type() == FileTransfer::Error::Type::ALREADY_EXISTS) ?
                FileTransfer::Error::ACTION_REPLACE_ALL : FileTransfer::Error::ACTION_SKIP_ALL;
        const FileTransfer::Error::Type type = error.type();
        QMetaObject::invokeMethod(transfer, [transfer, type, action]()
        {
            transfer->setAction(type, action);
        }, Qt::QueuedConnection);
    }, Qt::QueuedConnection);
    // The sheet is WA_DeleteOnClose and the user can dismiss it before the transfer finishes, so
    // it must be closed via its own connection (auto-broken on delete), not a captured pointer.
    connect(transfer, &FileTransfer::sig_finished, progress, &FileProgressSheet::close,
            Qt::QueuedConnection);
    connect(transfer, &FileTransfer::sig_finished, this, [this]()
    {
        local_panel_->refresh();
        remote_panel_->refresh();
    }, Qt::QueuedConnection);
    connect(progress, &FileProgressSheet::sig_cancel, transfer, &FileTransfer::stop, Qt::QueuedConnection);

    emit sig_transferRequest(transfer);
    progress->show();
}

//--------------------------------------------------------------------------------------------------
void FileTransferWindow::removeItems(FilePanelWidget* sender, const FileRemover::TaskList& items)
{
    FileRemover* remover = new FileRemover(sender->target(), items);
    remover->moveToThread(GuiApplication::ioThread());

    FileProgressSheet* progress = new FileProgressSheet(tr("Deleting"), this);
    progress->setAttribute(Qt::WA_DeleteOnClose);

    connect(remover, &FileRemover::sig_progressChanged, progress,
            [progress](const QString& name, int percentage)
    {
        progress->setCurrentItem(name);
        progress->setProgress(percentage);
    }, Qt::QueuedConnection);
    connect(remover, &FileRemover::sig_errorOccurred, this,
            [remover](const QString& /* path */, proto::file_transfer::ErrorCode /* error_code */,
                      quint32 /* available_actions */)
    {
        // MVP: skip on error so a single failure does not stall the queue.
        QMetaObject::invokeMethod(remover, [remover]()
        {
            remover->setAction(FileRemover::ACTION_SKIP_ALL);
        }, Qt::QueuedConnection);
    }, Qt::QueuedConnection);
    // The sheet is WA_DeleteOnClose and the user can dismiss it before the removal finishes, so
    // it must be closed via its own connection (auto-broken on delete), not a captured pointer.
    connect(remover, &FileRemover::sig_finished, progress, &FileProgressSheet::close,
            Qt::QueuedConnection);
    connect(remover, &FileRemover::sig_finished, this, [sender]()
    {
        sender->refresh();
    }, Qt::QueuedConnection);
    connect(progress, &FileProgressSheet::sig_cancel, remover, &FileRemover::stop, Qt::QueuedConnection);

    emit sig_removeRequest(remover);
    progress->show();
}

//--------------------------------------------------------------------------------------------------
void FileTransferWindow::setActivePanel(int index)
{
    active_panel_ = index;
    applyLayout();
}

//--------------------------------------------------------------------------------------------------
void FileTransferWindow::applyLayout()
{
    // Landscape shows both panels side by side; portrait shows one at a time with the switcher.
    const bool landscape = width() > height();

    tab_bar_->setVisible(!landscape);

    if (landscape)
    {
        local_panel_->setVisible(true);
        remote_panel_->setVisible(true);
    }
    else
    {
        local_panel_->setVisible(active_panel_ == 0);
        remote_panel_->setVisible(active_panel_ == 1);
    }

    tab_bar_->setCurrentIndex(active_panel_);
}

//--------------------------------------------------------------------------------------------------
void FileTransferWindow::setStatusText(const QString& text)
{
    status_->setText(text);
    status_->setVisible(true);
}
