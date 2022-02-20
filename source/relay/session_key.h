//
// Aspia Project
// Copyright (C) 2016-2022 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef RELAY_SESSION_KEY_H
#define RELAY_SESSION_KEY_H

#include "base/crypto/key_pair.h"

namespace relay {

class SessionKey
{
public:
    SessionKey();
    SessionKey(SessionKey&& other) noexcept;
    SessionKey& operator=(SessionKey&& other) noexcept;
    ~SessionKey();

    static SessionKey create();

    bool isValid() const;

    base::ByteArray privateKey() const;
    base::ByteArray publicKey() const;
    base::ByteArray sessionKey(std::string_view peer_public_key) const;
    base::ByteArray iv() const;

private:
    SessionKey(base::KeyPair&& key_pair, base::ByteArray&& iv);

    base::KeyPair key_pair_;
    base::ByteArray iv_;

    DISALLOW_COPY_AND_ASSIGN(SessionKey);
};

} // namespace relay

#endif // RELAY_SESSION_KEY_H
