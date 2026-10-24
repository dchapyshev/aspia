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

#ifndef BASE_PEER_HOST_KEY_H
#define BASE_PEER_HOST_KEY_H

#include <QByteArray>
#include <QString>

// Helpers for the persistent X25519 key of a host used by IDENTIFY_PASSWORD.
class HostKey
{
public:
    static const qsizetype kPublicKeySize = 32;

    // Human-readable fingerprint of a public key: SHA-256, upper-case hex, groups of four
    // characters ("1A2B 3C4D ..."). Returns an empty string for an invalid key.
    static QString fingerprint(const QByteArray& public_key);

private:
    HostKey() = delete;
};

#endif // BASE_PEER_HOST_KEY_H
