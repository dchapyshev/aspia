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

#ifndef HOST_WIN_PORTABLE_FILE_CLIENT_H
#define HOST_WIN_PORTABLE_FILE_CLIENT_H

#include <QPointer>

#include "base/power_save_blocker.h"
#include "host/client.h"

class PortableFileWorker;

// File transfer client of the portable host running with the rights of the user.
class PortableFileClient final : public Client
{
    Q_OBJECT

public:
    explicit PortableFileClient(TcpChannel* tcp_channel, QObject* parent = nullptr);
    ~PortableFileClient() final;

protected:
    // Client implementation.
    void onStart() final;
    void onMessage(quint8 channel_id, const QByteArray& buffer) final;

private:
    QPointer<PortableFileWorker> file_worker_;
    PowerSaveBlocker power_save_blocker_;
    bool is_session_locked_ = false;

    Q_DISABLE_COPY_MOVE(PortableFileClient)
};

#endif // HOST_WIN_PORTABLE_FILE_CLIENT_H
