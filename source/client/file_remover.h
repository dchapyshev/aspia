//
// PROJECT:         Aspia
// FILE:            client/file_remover.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__FILE_REMOVER_H
#define _ASPIA_CLIENT__FILE_REMOVER_H

#include <QObject>
#include <QQueue>

#include "client/file_remove_task.h"
#include "client/file_reply_receiver.h"
#include "proto/file_transfer_session.pb.h"

namespace aspia {

class FileRemoveQueueBuilder;

class FileRemover : public QObject
{
    Q_OBJECT

public:
    explicit FileRemover(QObject* parent = nullptr);
    ~FileRemover();

    enum Action
    {
        Ask     = 0,
        Abort   = 1,
        Skip    = 2,
        SkipAll = 4
    };
    Q_DECLARE_FLAGS(Actions, Action)

signals:
    void started();
    void finished();
    void progressChanged(const QString& name, int percentage);
    void error(FileRemover* remover, FileRemover::Actions actions, const QString& message);
    void request(const proto::file_transfer::Request& request,
                 const FileReplyReceiver& receiver);

public slots:
    void start(const QList<FileRemoveTask>& tasks);
    void applyAction(Action action);
    void reply(const proto::file_transfer::Request& request,
               const proto::file_transfer::Reply& reply);

private slots:
    void taskQueueError(const QString& message);
    void taskQueueReady();

private:
    void processTask();
    void processNextTask();

    FileRemoveQueueBuilder* builder_ = nullptr;
    QQueue<FileRemoveTask> tasks_;

    Action failure_action_ = Action::Ask;
    int tasks_count_ = 0;

    Q_DISABLE_COPY(FileRemover)
};

Q_DECLARE_OPERATORS_FOR_FLAGS(FileRemover::Actions)

} // namespace aspia

#endif // _ASPIA_CLIENT__FILE_REMOVER_H
