//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef ASPIA_HOST__HOST_SESSION_H_
#define ASPIA_HOST__HOST_SESSION_H_

#include <QByteArray>
#include <QPointer>

namespace aspia {

class IpcChannel;

class HostSession : public QObject
{
    Q_OBJECT

public:
    virtual ~HostSession() = default;

    static HostSession* create(const QString& session_type, const QString& channel_id);

    void start();

public slots:
    virtual void messageReceived(const QByteArray& buffer) = 0;
    virtual void messageWritten(int message_id) = 0;

signals:
    void writeMessage(int message_id, const QByteArray& buffer);
    void readMessage();
    void errorOccurred();

protected:
    explicit HostSession(const QString& channel_id);

    virtual void startSession() = 0;
    virtual void stopSession() = 0;

private slots:
    void ipcChannelConnected();
    void stop();

private:
    QString channel_id_;
    QPointer<IpcChannel> ipc_channel_;

    Q_DISABLE_COPY(HostSession)
};

} // namespace aspia

#endif // ASPIA_HOST__HOST_SESSION_H_
