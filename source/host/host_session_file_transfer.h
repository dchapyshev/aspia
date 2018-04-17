//
// PROJECT:         Aspia
// FILE:            host/host_session_file_transfer.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_HOST__HOST_SESSION_FILE_TRANSFER_H
#define _ASPIA_HOST__HOST_SESSION_FILE_TRANSFER_H

#include <QPointer>

#include "host/host_session.h"

namespace aspia {

class FileWorker;

class HostSessionFileTransfer : public HostSession
{
    Q_OBJECT

public:
    explicit HostSessionFileTransfer(const QString& channel_id);
    ~HostSessionFileTransfer() = default;

public slots:
    // HostSession implementation.
    void readMessage(const QByteArray& buffer) override;
    void messageWritten(int message_id) override;

protected:
    void startSession() override;
    void stopSession() override;

private:
    QPointer<FileWorker> worker_;

    Q_DISABLE_COPY(HostSessionFileTransfer)
};

} // namespace aspia

#endif // _ASPIA_HOST__HOST_SESSION_FILE_TRANSFER_H
