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

#ifndef HOST_CREDENTIALS_H
#define HOST_CREDENTIALS_H

#include <QObject>

#include "base/scoped_qpointer.h"
#include "base/crypto/secure_string.h"

class IpcChannel;
class IpcServer;

class Credentials final : public QObject
{
    Q_OBJECT

public:
    explicit Credentials(QObject* parent = nullptr);
    ~Credentials() final;

    bool start();
    bool sendCredentials(const SecureString& username, const SecureString& password);

signals:
    void sig_connected(quint32 screen_type);

private slots:
    // Slots for IpcServer.
    void onIpcNewConnection();
    void onIpcErrorOccurred();

    // Slots for IpcChannel.
    void onIpcMessageReceived(quint32 channel_id, const QByteArray& buffer, bool reliable);
    void onIpcDisconnected();

private:
    ScopedQPointer<IpcServer> ipc_server_;
    ScopedQPointer<IpcChannel> ipc_channel_;

    Q_DISABLE_COPY_MOVE(Credentials)
};

#endif // HOST_CREDENTIALS_H
