//
// PROJECT:         Aspia
// FILE:            client/ui/file_manager_window.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__UI__FILE_MANAGER_WINDOW_H
#define _ASPIA_CLIENT__UI__FILE_MANAGER_WINDOW_H

#include "base/macros.h"
#include "client/file_reply_receiver.h"
#include "proto/file_transfer_session.pb.h"
#include "proto/computer.pb.h"
#include "qt/ui_file_manager_window.h"

namespace aspia {

class FileManagerWindow : public QWidget
{
    Q_OBJECT

public:
    FileManagerWindow(proto::Computer* computer, QWidget* parent = nullptr);
    ~FileManagerWindow() = default;

signals:
    void windowClose();
    void localRequest(const proto::file_transfer::Request& request,
                      const FileReplyReceiver& receiver);
    void remoteRequest(const proto::file_transfer::Request& request,
                       const FileReplyReceiver& receiver);

public slots:
    void localReply(const proto::file_transfer::Request& request,
                    const proto::file_transfer::Reply& reply);
    void remoteReply(const proto::file_transfer::Request& request,
                     const proto::file_transfer::Reply& reply);

private slots:
    void localFileListRequest(const QString& directory_path);
    void remoteFileListRequest(const QString& directory_path);

private:
    void closeEvent(QCloseEvent* event) override;

    Ui::FileManagerWindow ui;

    DISALLOW_COPY_AND_ASSIGN(FileManagerWindow);
};

} // namespace aspia

#endif // _ASPIA_CLIENT__UI__FILE_MANAGER_WINDOW_H
