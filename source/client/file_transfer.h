//
// PROJECT:         Aspia
// FILE:            client/file_transfer.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__FILE_TRANSFER_H
#define _ASPIA_CLIENT__FILE_TRANSFER_H

#include <QObject>

#include "client/file_reply_receiver.h"
#include "proto/file_transfer_session.pb.h"

namespace aspia {

class FileTransfer : public QObject
{
    Q_OBJECT

public:
    explicit FileTransfer(QObject* parent = nullptr);
    virtual ~FileTransfer() = default;

    enum Action
    {
        Ask        = 0,
        Abort      = 1,
        Skip       = 2,
        SkipAll    = 4,
        Replace    = 5,
        ReplaceAll = 6
    };
    Q_DECLARE_FLAGS(Actions, Action)

    Action directoryCreateErrorAction() const;
    void setDirectoryCreateErrorAction(Action action);

    Action fileCreateErrorAction() const;
    void setFileCreateErrorAction(Action action);

    Action fileExistsAction() const;
    void setFileExistsAction(Action action);

    Action fileOpenErrorAction() const;
    void setFileOpenErrorAction(Action action);

    Action fileReadErrorAction() const;
    void setFileReadErrorAction(Action action);

signals:
    void started();
    void finished();
    void error(FileTransfer* transfer, FileTransfer::Actions actions, const QString& message);
    void localRequest(const proto::file_transfer::Request& request,
                      const FileReplyReceiver& receiver);
    void remoteRequest(const proto::file_transfer::Request& request,
                       const FileReplyReceiver& receiver);

public slots:
    virtual void start(const QString& source_path,
                       const QString& target_path,
                       const QList<QPair<QString, bool>>& items) = 0;
    virtual void applyAction(Action action) = 0;

private:
    Action action_directory_create_error_ = Action::Ask;
    Action action_file_create_error_ = Action::Ask;
    Action action_file_exists_ = Action::Ask;
    Action action_file_open_error_ = Action::Ask;
    Action action_file_read_error_ = Action::Ask;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(FileTransfer::Actions)

} // namespace aspia

#endif // _ASPIA_CLIENT__FILE_TRANSFER_H
