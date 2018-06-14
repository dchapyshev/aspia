//
// PROJECT:         Aspia
// FILE:            host/host_session_fake.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/host_session_fake.h"

#include "host/host_session_fake_desktop.h"
#include "host/host_session_fake_file_transfer.h"

namespace aspia {

HostSessionFake::HostSessionFake(QObject* parent)
    : QObject(parent)
{
    // Nothing
}

// static
HostSessionFake* HostSessionFake::create(proto::auth::SessionType session_type, QObject* parent)
{
    switch (session_type)
    {
        case proto::auth::SESSION_TYPE_DESKTOP_MANAGE:
        case proto::auth::SESSION_TYPE_DESKTOP_VIEW:
            return new HostSessionFakeDesktop(parent);

        case proto::auth::SESSION_TYPE_FILE_TRANSFER:
            return new HostSessionFakeFileTransfer(parent);

        default:
            return nullptr;
    }
}

} // namespace aspia
