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

#ifndef HOST_DOMAIN_LOGON_H
#define HOST_DOMAIN_LOGON_H

#include <QString>
#include <QVector>

#include "base/peer/domain_authenticator.h"
#include "host/domain_group_rule.h"

class SecureString;

// A group the account is a member of, as reported by the operating system.
struct DomainLogonGroup
{
    // Windows: string SID ("S-1-5-21-..."). Linux: numeric GID ("1234"), or "uid:1234" for the
    // account itself.
    QString id;

    // True for the entry of the account itself (its own SID / uid). It is matched only by rules for
    // a single user, never by group rules.
    bool is_account = false;
};

struct DomainLogonResult
{
    DomainAuthenticator::Status status = DomainAuthenticator::Status::INTERNAL_ERROR;
    QString account; // Canonical account name for logs.
    QVector<DomainLogonGroup> groups;
};

// Verifies the credentials with the operating system and returns the groups of the account.
//   Windows: LogonUserW(LOGON32_LOGON_NETWORK) + GetTokenInformation(TokenGroups). The computer
//            must be joined to the domain; local accounts work too (empty domain or ".").
//   Linux:   PAM (pam_authenticate + pam_acct_mgmt) with |pam_service|, then getgrouplist().
//            The computer must be joined to the domain with sssd or winbind. |peer_address| is
//            passed as PAM_RHOST (pam_faillock, pam_access and the audit log use it).
//   macOS:   not supported.
// Only identifiers are returned: rules are matched by identifier, so no name lookups are needed.
// Blocking and thread-safe.
DomainLogonResult domainLogon(const QString& domain,
                              const QString& username,
                              const SecureString& password,
                              const QString& peer_address,
                              const QString& pam_service);

struct DomainAccountInfo
{
    QString id;   // SID, GID or "uid:N".
    QString name; // Canonical name for display ("CORP\Aspia Operators", "aspia-ops"); may be empty.
    DomainGroupRule::Kind kind = DomainGroupRule::Kind::GROUP;
};

// Resolves a group (or a single user) as the administrator entered it into its identifier.
//   Windows: a SID, "DOMAIN\name", or a name without a domain. A name without a domain is accepted
//            only if it belongs to this computer, its built-in groups, well-known groups or the
//            domain the computer is joined to - never to a trusted domain, which must be written out.
//            Computer accounts are refused.
//   Linux:   a GID, "uid:N" (a single user) or a group name as NSS knows it (sssd/winbind resolve a
//            name without a domain in the default domain only).
// Returns false and sets |error| if the account can not be found. Blocking and thread-safe (may ask
// a domain controller).
bool resolveDomainAccount(const QString& text, DomainAccountInfo* info, QString* error);

// Resolves a rule saved without an identifier by an older version, with the rules of that version
// so that an upgrade never changes who gets access. Linux: "uid:N", a GID or a group name exactly
// as typed (never a user, no other spellings); logs a warning if the name would now also match a
// user. Windows: the same as resolveDomainAccount() (unchanged there).
bool resolveLegacyDomainRule(const QString& text, DomainAccountInfo* info, QString* error);

// True if |name| is a syntactically valid PAM service name: 1..64 characters of [A-Za-z0-9._-],
// not starting with a dot. Platform independent.
bool isValidPamServiceName(const QString& name);

// Linux: the name is valid and its file exists in /etc/pam.d (or the vendor PAM directory).
// Other platforms: always true (PAM is not used). Sets |error| on failure.
bool isUsablePamService(const QString& name, QString* error);

#endif // HOST_DOMAIN_LOGON_H
