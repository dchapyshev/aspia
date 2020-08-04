//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "peer/authenticator.h"

#include "base/logging.h"

namespace peer {

// static
const char* Authenticator::osTypeToString(proto::OsType os_type)
{
    switch (os_type)
    {
        case proto::OS_TYPE_WINDOWS:
            return "Windows";

        case proto::OS_TYPE_LINUX:
            return "Linux";

        case proto::OS_TYPE_ANDROID:
            return "Android";

        case proto::OS_TYPE_OSX:
            return "OSX";

        case proto::OS_TYPE_IOS:
            return "IOS";

        default:
            return "Unknown";
    }
}

void Authenticator::onConnected()
{
    // The authenticator receives the channel always in an already connected state.
    NOTREACHED();
}

} // namespace peer
