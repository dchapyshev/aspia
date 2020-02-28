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

#ifndef NET__NETWORK_ERROR_H
#define NET__NETWORK_ERROR_H

#include <string>

namespace net {

enum class ErrorCode
{
    // Unknown error.
    UNKNOWN,

    // Cryptography error (message encryption or decryption failed).
    ACCESS_DENIED,

    // An error occurred with the network (e.g., the network cable was accidentally plugged out).
    NETWORK_ERROR,

    // The connection was refused by the peer (or timed out).
    CONNECTION_REFUSED,

    // The remote host closed the connection.
    REMOTE_HOST_CLOSED,

    // The host address was not found.
    SPECIFIED_HOST_NOT_FOUND,

    // The socket operation timed out.
    SOCKET_TIMEOUT,

    // The address specified is already in use and was set to be exclusive.
    ADDRESS_IN_USE,

    // The address specified does not belong to the host.
    ADDRESS_NOT_AVAILABLE
};

// Converts an error code to a human readable string.
// Does not support localization. Used for logs.
std::string errorToString(ErrorCode error_code);

} // namespace net

#endif // NET__NETWORK_ERROR_H
