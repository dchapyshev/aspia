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

#ifndef BASE_PEER_DOMAIN_AUTHENTICATOR_H
#define BASE_PEER_DOMAIN_AUTHENTICATOR_H

#include <QString>

class SecureString;

// Verifies the credentials of an operating system or domain account (Active Directory through
// LogonUserW on Windows, PAM with sssd/winbind on Linux) and returns the session types the account
// is allowed to use.
//
// authenticate() blocks (a domain controller may take seconds to answer) and is called on a thread
// pool thread. Implementations must be thread-safe and must not use per-thread state such as the
// host database: everything they need is captured when the object is created.
class DomainAuthenticator
{
public:
    virtual ~DomainAuthenticator() = default;

    enum class Status
    {
        SUCCESS,             // Credentials are valid and the account has at least one session type.
        INVALID_CREDENTIALS, // Unknown account or wrong password.
        ACCOUNT_DISABLED,    // The account is disabled.
        ACCOUNT_LOCKED,      // The account is locked out.
        ACCOUNT_EXPIRED,     // The account has expired.
        CREDENTIALS_EXPIRED, // The password has expired or must be changed.
        LOGON_DENIED,        // The account may not log on to this computer (policy, logon hours).
        NO_PERMISSIONS,      // Credentials are valid, but no group of the account is mapped.
        UNSUPPORTED,         // Not supported on this platform or not configured.
        INTERNAL_ERROR       // Any other error.
    };

    struct Result
    {
        Status status = Status::INTERNAL_ERROR;
        QString account;          // Canonical account name for logs, e.g. "CORP\ivanov".
        quint32 session_types = 0; // Bitmask of proto::peer::SessionType.
    };

    // |peer_address| is the IP address of the client (may be empty). It is passed to the operating
    // system for its own logging and lockout policy (PAM_RHOST on Linux).
    [[nodiscard]] virtual Result authenticate(const QString& domain,
                                              const QString& username,
                                              const SecureString& password,
                                              const QString& peer_address) const = 0;

    // Splits "DOMAIN\user" into |domain| and |user|. "user@domain" and "user" are returned unchanged
    // in |user| with an empty |domain| (the UPN form is passed to the operating system as is).
    static void splitAccountName(const QString& account, QString* domain, QString* user);

    static const char* statusToString(Status status);
};

#endif // BASE_PEER_DOMAIN_AUTHENTICATOR_H
