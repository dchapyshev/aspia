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

namespace {

QString statusToString(proto::file_transfer::Status status)
{
    switch (status)
    {
        case proto::file_transfer::STATUS_SUCCESS:
            return QCoreApplication::tr("Successfully completed");

        case proto::file_transfer::STATUS_INVALID_REQUEST:
            return QCoreApplication::tr("Invalid request");

        case proto::file_transfer::STATUS_INVALID_PATH_NAME:
            return QCoreApplication::tr("Invalid directory or file name");

        case proto::file_transfer::STATUS_PATH_NOT_FOUND:
            return QCoreApplication::tr("Path not found");

        case proto::file_transfer::STATUS_PATH_ALREADY_EXISTS:
            return QCoreApplication::tr("Path already exists");

        case proto::file_transfer::STATUS_NO_DRIVES_FOUND:
            return QCoreApplication::tr("No drives found");

        case proto::file_transfer::STATUS_DISK_FULL:
            return QCoreApplication::tr("Disk full");

        case proto::file_transfer::STATUS_ACCESS_DENIED:
            return QCoreApplication::tr("Access denied");

        case proto::file_transfer::STATUS_FILE_OPEN_ERROR:
            return QCoreApplication::tr("Could not open file for reading");

        case proto::file_transfer::STATUS_FILE_CREATE_ERROR:
            return QCoreApplication::tr("Could not create or replace file");

        case proto::file_transfer::STATUS_FILE_WRITE_ERROR:
            return QCoreApplication::tr("Could not write to file");

        case proto::file_transfer::STATUS_FILE_READ_ERROR:
            return QCoreApplication::tr("Could not read file");

        default:
            return QCoreApplication::tr("Unknown status code");
    }
}

} // namespace

FileManagerWindow::FileManagerWindow(proto::Computer* computer, QWidget* parent)
    : QWidget(parent)
{
    ui.setupUi(this);

    QString computer_name;
    if (!computer->name().empty())
    {
        computer_name = QString::fromUtf8(computer->name().c_str(),
                                          computer->name().size());
    }
    else
    {
        computer_name = QString::fromUtf8(computer->address().c_str(),
                                          computer->address().size());
    }

    setWindowTitle(tr("%1 - Aspia File Transfer").arg(computer_name));

    QList<int> sizes;
    sizes.push_back(width() / 2);
    sizes.push_back(width() / 2);

    ui.splitter->setSizes(sizes);

    connect(ui.address_bar_local, SIGNAL(addressChanged(const QString&)),
            this, SLOT(localFileListRequest(const QString&)));

    connect(ui.address_bar_remote, SIGNAL(addressChanged(const QString&)),
            this, SLOT(remoteFileListRequest(const QString&)));

    connect(ui.tree_local, SIGNAL(addressChanged(const QString&)),
            ui.address_bar_local, SLOT(toChildFolder(const QString&)));

    connect(ui.tree_remote, SIGNAL(addressChanged(const QString&)),
            ui.address_bar_remote, SLOT(toChildFolder(const QString&)));

    connect(ui.button_local_up, SIGNAL(pressed()), ui.address_bar_local, SLOT(toParentFolder()));
    connect(ui.button_local_refresh, SIGNAL(pressed()), ui.address_bar_local, SLOT(refresh()));

    connect(ui.button_remote_up, SIGNAL(pressed()), ui.address_bar_remote, SLOT(toParentFolder()));
    connect(ui.button_remote_refresh, SIGNAL(pressed()), ui.address_bar_remote, SLOT(refresh()));
}

void FileManagerWindow::localReply(const proto::file_transfer::Request& request,
                                   const proto::file_transfer::Reply& reply)
{
    if (reply.status() != proto::file_transfer::STATUS_SUCCESS)
    {
        QMessageBox::warning(this, tr("Warning"), statusToString(reply.status()), QMessageBox::Ok);
        return;
    }

    if (request.has_drive_list_request())
    {
        ui.address_bar_local->updateDrives(reply.drive_list());
    }
    else if (request.has_file_list_request())
    {
        ui.tree_local->updateFiles(reply.file_list());
    }
}

void FileManagerWindow::remoteReply(const proto::file_transfer::Request& request,
                                    const proto::file_transfer::Reply& reply)
{
    if (reply.status() != proto::file_transfer::STATUS_SUCCESS)
    {
        QMessageBox::warning(this, tr("Warning"), statusToString(reply.status()), QMessageBox::Ok);
        return;
    }

    if (request.has_drive_list_request())
    {
        ui.address_bar_remote->updateDrives(reply.drive_list());
    }
    else if (request.has_file_list_request())
    {
        ui.tree_remote->updateFiles(reply.file_list());
    }
}

void FileManagerWindow::localFileListRequest(const QString& directory_path)
{
    emit localRequest(FileRequest::fileListRequest(directory_path),
                      FileReplyReceiver(this, "localReply"));
}

void FileManagerWindow::remoteFileListRequest(const QString& directory_path)
{
    emit remoteRequest(FileRequest::fileListRequest(directory_path),
                       FileReplyReceiver(this, "remoteReply"));
}

void FileManagerWindow::closeEvent(QCloseEvent* event)
{
    emit windowClose();
    QWidget::closeEvent(event);
}

} // namespace aspia
