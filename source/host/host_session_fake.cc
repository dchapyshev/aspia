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

#include "host/host_session_fake.h"
#include "host/host_session_fake_desktop.h"
#include "host/host_session_fake_file_transfer.h"

namespace host {

SessionFake::SessionFake(QObject* parent)
    : QObject(parent)
{
    // Nothing
}

// static
SessionFake* SessionFake::create(proto::SessionType session_type, QObject* parent)
{
    switch (session_type)
    {
        case proto::SESSION_TYPE_DESKTOP_MANAGE:
        case proto::SESSION_TYPE_DESKTOP_VIEW:
            return new SessionFakeDesktop(parent);

        case proto::SESSION_TYPE_FILE_TRANSFER:
            return new SessionFakeFileTransfer(parent);

        default:
            return nullptr;
    }
}

} // namespace host
