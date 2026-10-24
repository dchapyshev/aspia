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

#ifndef HOST_HOST_DOMAIN_AUTHENTICATOR_H
#define HOST_HOST_DOMAIN_AUTHENTICATOR_H

#include <QVector>

#include <functional>
#include <mutex>

#include "base/peer/domain_authenticator.h"
#include "host/domain_group_rule.h"
#include "host/domain_logon.h"

class HostDomainAuthenticator final : public DomainAuthenticator
{
public:
    using Resolver = std::function<bool(const QString& text, DomainAccountInfo* info, QString* error)>;

    // |resolver| resolves rules saved without an identifier by older versions (resolveLegacyDomainRule()
    // by default).
    HostDomainAuthenticator(const QVector<DomainGroupRule>& rules, const QString& pam_service,
                            Resolver resolver = nullptr);
    ~HostDomainAuthenticator() final = default;

    // DomainAuthenticator implementation.
    Result authenticate(const QString& domain,
                        const QString& username,
                        const SecureString& password,
                        const QString& peer_address) const final;

    // Bitmask of session types granted by |rules| to an account with |groups|.
    static quint32 sessionsForGroups(const QVector<DomainGroupRule>& rules,
                                     const QVector<DomainLogonGroup>& groups);

    // True when |rule| names |group|: the identifiers are equal and a group rule meets a group, a
    // user rule meets the account itself. Names are never compared.
    static bool isMatch(const DomainGroupRule& rule, const DomainLogonGroup& group);

    // True when the group with identifier |id| (and canonical |name|, may be empty) contains
    // practically every account: Everyone, Authenticated Users, Network, Interactive, BUILTIN\Users,
    // BUILTIN\Guests, Domain Users, Domain Guests (Windows) or "users" / GID 100 (Linux). Mapping
    // such a group gives every account of the domain access to this computer.
    static bool isBroadGroup(const QString& id, const QString& name);

    // Rules with identifiers. Rules saved without one are resolved with |resolver| (a warning is
    // logged); rules that can not be resolved are dropped (they never match).
    static QVector<DomainGroupRule> resolvedRules(const QVector<DomainGroupRule>& rules,
                                                  const Resolver& resolver);

private:
    const QVector<DomainGroupRule> rules_;
    const QString pam_service_;
    const Resolver resolver_;

    // Legacy rules are resolved once per snapshot, on the first logon.
    mutable std::once_flag resolve_once_;
    mutable QVector<DomainGroupRule> resolved_rules_;

    Q_DISABLE_COPY_MOVE(HostDomainAuthenticator)
};

#endif // HOST_HOST_DOMAIN_AUTHENTICATOR_H
