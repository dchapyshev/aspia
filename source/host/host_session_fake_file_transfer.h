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

#ifndef ASPIA_HOST__HOST_SESSION_FAKE_FILE_TRANSFER_H_
#define ASPIA_HOST__HOST_SESSION_FAKE_FILE_TRANSFER_H_

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

private:
    Q_DISABLE_COPY(HostSessionFakeFileTransfer)
};

} // namespace aspia

#endif // ASPIA_HOST__HOST_SESSION_FAKE_FILE_TRANSFER_H_
