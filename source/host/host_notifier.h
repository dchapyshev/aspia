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

#ifndef ASPIA_HOST__HOST_NOTIFIER_H_
#define ASPIA_HOST__HOST_NOTIFIER_H_

#include "ipc/ipc_channel.h"
#include "notifier.pb.h"

namespace aspia {

class HostNotifier : public QObject
{
    Q_OBJECT

public:
    explicit HostNotifier(QObject* parent = nullptr);
    ~HostNotifier() = default;

    bool start(const QString& channel_id);

public slots:
    void stop();
    void killSession(const std::string& uuid);

signals:
    void finished();
    void sessionOpen(const proto::notifier::Session& session);
    void sessionClose(const proto::notifier::SessionClose& session_close);

private slots:
    void onIpcMessageReceived(const QByteArray& buffer);

private:
    QPointer<IpcChannel> ipc_channel_;

    DISALLOW_COPY_AND_ASSIGN(HostNotifier);
};

} // namespace aspia

#endif // ASPIA_HOST__HOST_NOTIFIER_H_
