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

#ifndef ASPIA_HOST__HOST_SESSION_H
#define ASPIA_HOST__HOST_SESSION_H

#include <QByteArray>
#include <QPointer>

#include "base/macros_magic.h"

namespace ipc {
class Channel;
} // namespace ipc

namespace host {

class HostSession : public QObject
{
    Q_OBJECT

public:
    virtual ~HostSession() = default;

    static HostSession* create(const QString& session_type, const QString& channel_id);

    void start();

public slots:
    virtual void messageReceived(const QByteArray& buffer) = 0;

signals:
    void sendMessage(const QByteArray& buffer);
    void errorOccurred();

protected:
    explicit HostSession(const QString& channel_id);

private slots:
    virtual void startSession() = 0;
    virtual void stopSession() = 0;
    void stop();

private:
    QString channel_id_;
    QPointer<ipc::Channel> ipc_channel_;

    DISALLOW_COPY_AND_ASSIGN(HostSession);
};

} // namespace host

#endif // ASPIA_HOST__HOST_SESSION_H
