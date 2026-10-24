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

#ifndef BASE_PEER_USER_H
#define BASE_PEER_USER_H

#include <QByteArray>
#include <QString>

#include <string>

class SecureString;

class User
{
public:
    User() = default;
    ~User() = default;

    User(const User& other) = default;
    User& operator=(const User& other) = default;

    User(User&& other) noexcept = default;
    User& operator=(User&& other) noexcept = default;

    enum Flags { ENABLED = 1 };

    static const size_t kMaxUserNameLength = 64;
    // An operating system or domain account: "DOMAIN\user", "user@domain.tld" or "user".
    static const size_t kMaxDomainUserNameLength = 256;
    static const size_t kMaxDomainPasswordLength = 256;
    static const size_t kMinPasswordLength = 8;
    static const size_t kMaxPasswordLength = 64;
    static const size_t kSafePasswordLength = 10;

    // The salt is 64 bytes, the verifier at most as wide as the modulus of the widest SRP group
    // (8192 bits). Bounded because the record is stored as the peer sends it and travels back in
    // the user list.
    static const size_t kMaxSaltSize = 128;
    static const size_t kMaxVerifierSize = 1024;

    static bool isValidUserName(const QString& username);

    // Validates the name of an operating system or domain account. Unlike isValidUserName() it
    // also accepts the down-level logon name "DOMAIN\user" (a single backslash) and spaces inside
    // the name, which Active Directory allows in sAMAccountName. Computer accounts (a name that
    // ends with a dollar sign) are refused: people never use them and the domain manages their
    // passwords.
    static bool isValidDomainUserName(const QString& username);

    // True when |bytes| (a field of PasswordCredentials as received) is well-formed UTF-8 without
    // NUL characters. A NUL would cut the string short in the C APIs of the operating system, and
    // malformed UTF-8 would be replaced by U+FFFD and could make two different inputs equal.
    static bool isValidCredentialText(const std::string& bytes);
    static bool isValidPassword(const SecureString& password);
    static bool isSafePassword(const SecureString& password);

    static User create(const QString& name, const SecureString& password);
    bool isValid() const;

    static const User kInvalidUser;

    qint64 entry_id = 0;
    QString name;
    QString group;
    QByteArray salt;
    QByteArray verifier;
    quint32 sessions = 0;
    quint32 flags = 0;
};

#endif // BASE_PEER_USER_H
