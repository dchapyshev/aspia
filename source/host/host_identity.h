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

#ifndef HOST_HOST_IDENTITY_H
#define HOST_HOST_IDENTITY_H

#include <QByteArray>

class Database;
class SecureByteArray;

// The persistent X25519 key pair that identifies the host to clients connecting with an operating
// system / domain account (IDENTIFY_PASSWORD). Clients pin its public key on the first connection.
class HostIdentity
{
public:
    // Returns the private key stored in |database|, generating and storing a new one if there is
    // none yet. A stored key that can not be read (damaged, or protected on another computer) is
    // NOT replaced silently: an empty array is returned, operating system / domain accounts stay
    // unavailable and the administrator regenerates the key in the settings (clients then see a
    // changed-key warning, which is exactly what they should see).
    //
    // Windows: the key is stored protected with DPAPI (machine scope), so a copy of the database
    // is useless on another computer. Other platforms: stored as is; the database file is 0600.
    static SecureByteArray privateKey(Database& database);

    // Generates and stores a new key, replacing the stored one. Returns false on failure.
    static bool regenerate(Database& database);

    // Public key matching privateKey().
    static QByteArray publicKey(Database& database);

private:
    HostIdentity() = delete;
};

#endif // HOST_HOST_IDENTITY_H
