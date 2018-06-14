//
// PROJECT:         Aspia
// FILE:            host/host_session_fake_file_transfer.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_HOST__HOST_SESSION_FAKE_FILE_TRANSFER_H
#define _ASPIA_HOST__HOST_SESSION_FAKE_FILE_TRANSFER_H

#include "host/host_session_fake.h"

namespace aspia {

class HostSessionFakeFileTransfer : public HostSessionFake
{
    Q_OBJECT

public:
    explicit HostSessionFakeFileTransfer(QObject* parent);

    // HostSessionFake implementation.
    void startSession() override;

public slots:
    // HostSessionFake implementation.
    void onMessageReceived(const QByteArray& buffer) override;
    void onMessageWritten(int message_id) override;

private:
    Q_DISABLE_COPY(HostSessionFakeFileTransfer)
};

} // namespace aspia

#endif // _ASPIA_HOST__HOST_SESSION_FAKE_FILE_TRANSFER_H
