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

#include "host/host_domain_authenticator.h"

#include <QMutex>
#include <QSet>

#include "base/logging.h"
#include "base/crypto/secure_string.h"
#include "proto/peer.h"

//--------------------------------------------------------------------------------------------------
HostDomainAuthenticator::HostDomainAuthenticator(
    const QVector<DomainGroupRule>& rules, const QString& pam_service, Resolver resolver)
    : rules_(rules),
      pam_service_(pam_service),
      resolver_(resolver ? std::move(resolver) : Resolver(&resolveLegacyDomainRule))
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
DomainAuthenticator::Result HostDomainAuthenticator::authenticate(
    const QString& domain, const QString& username, const SecureString& password,
    const QString& peer_address) const
{
    Result result;

    if (username.isEmpty() || password.isEmpty())
    {
        result.status = Status::INVALID_CREDENTIALS;
        return result;
    }

    DomainLogonResult logon = domainLogon(domain, username, password, peer_address, pam_service_);

    result.status = logon.status;
    result.account = logon.account;

    if (logon.status != Status::SUCCESS)
        return result;

    std::call_once(resolve_once_, [this]()
    {
        resolved_rules_ = resolvedRules(rules_, resolver_);
    });

    result.session_types = sessionsForGroups(resolved_rules_, logon.groups);
    if (!result.session_types)
    {
        LOG(INFO) << "No group of" << logon.account << "is mapped (groups:" << logon.groups.size()
                  << ")";
        result.status = Status::NO_PERMISSIONS;
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
// static
bool HostDomainAuthenticator::isBroadGroup(const QString& id, const QString& name)
{
    static const char* const kBroadSids[] =
    {
        "S-1-1-0",      // Everyone
        "S-1-2-0",      // LOCAL
        "S-1-2-1",      // CONSOLE LOGON
        "S-1-5-1",      // DIALUP
        "S-1-5-2",      // NETWORK (every network logon)
        "S-1-5-3",      // BATCH
        "S-1-5-4",      // INTERACTIVE
        "S-1-5-6",      // SERVICE
        "S-1-5-7",      // ANONYMOUS LOGON
        "S-1-5-11",     // Authenticated Users
        "S-1-5-13",     // TERMINAL SERVER USER
        "S-1-5-14",     // REMOTE INTERACTIVE LOGON
        "S-1-5-15",     // This Organization
        "S-1-5-1000",   // Other Organization
        "S-1-5-113",    // Local account
        "S-1-5-114",    // Local account and member of Administrators group
        "S-1-5-64-10",  // NTLM Authentication
        "S-1-5-64-14",  // SChannel Authentication
        "S-1-5-64-21",  // Digest Authentication
        "S-1-18-1",     // Authentication authority asserted identity
        "S-1-18-2",     // Service asserted identity
        "S-1-5-32-545", // BUILTIN\Users
        "S-1-5-32-546", // BUILTIN\Guests
        "S-1-5-32-547", // BUILTIN\Power Users
        "S-1-5-32-554", // BUILTIN\Pre-Windows 2000 Compatible Access
        "S-1-5-32-555", // BUILTIN\Remote Desktop Users
        "S-1-5-32-559"  // BUILTIN\Performance Log Users
    };

    const QString value = id.trimmed();

    for (const char* sid : kBroadSids)
    {
        if (value.compare(QLatin1String(sid), Qt::CaseInsensitive) == 0)
            return true;
    }

    // Domain Users (513), Domain Guests (514), Domain Computers (515) and the local Guest account
    // (501) of any domain or computer: S-1-5-21-a-b-c-RID.
    if (value.startsWith(QLatin1String("S-1-5-21-"), Qt::CaseInsensitive) &&
        (value.endsWith(QLatin1String("-513")) || value.endsWith(QLatin1String("-514")) ||
         value.endsWith(QLatin1String("-515")) || value.endsWith(QLatin1String("-501"))))
    {
        return true;
    }

    // Linux: the "users" group (GID 100 on Debian/Ubuntu/RHEL) and "nogroup"/"nobody" (65534).
    if (value == QLatin1String("100") || value == QLatin1String("65534"))
        return true;

    // By name, for Linux groups of a domain (sssd/winbind give them GIDs from a range).
    QString short_name = name.trimmed();
    const qsizetype backslash = short_name.lastIndexOf(QLatin1Char('\\'));
    if (backslash >= 0)
        short_name = short_name.mid(backslash + 1);
    const qsizetype at = short_name.indexOf(QLatin1Char('@'));
    if (at >= 0)
        short_name.truncate(at);

    static const char* const kBroadNames[] =
    {
        "users", "domain users", "domain guests", "domain computers", "everyone",
        "authenticated users", "guests", "this organization", "other organization",
        "local account", "network", "interactive", "remote desktop users", "nogroup", "nobody"
    };

    for (const char* broad_name : kBroadNames)
    {
        if (short_name.compare(QLatin1String(broad_name), Qt::CaseInsensitive) == 0)
            return true;
    }

    return false;
}

//--------------------------------------------------------------------------------------------------
// static
quint32 HostDomainAuthenticator::sessionsForGroups(const QVector<DomainGroupRule>& rules,
                                                   const QVector<DomainLogonGroup>& groups)
{
    quint32 sessions = 0;

    for (const DomainGroupRule& rule : rules)
    {
        if (!rule.sessions)
            continue;

        for (const DomainLogonGroup& group : groups)
        {
            if (isMatch(rule, group))
            {
                sessions |= rule.sessions;
                break;
            }
        }
    }

    return sessions & static_cast<quint32>(proto::peer::SESSION_TYPE_ALL);
}

//--------------------------------------------------------------------------------------------------
// static
bool HostDomainAuthenticator::isMatch(const DomainGroupRule& rule, const DomainLogonGroup& group)
{
    const QString rule_id = rule.id.trimmed();
    if (rule_id.isEmpty() || group.id.isEmpty())
        return false;

    // A group rule never matches the account itself (a user named like the group), and a user rule
    // never matches a group.
    const bool user_rule = (rule.kind == DomainGroupRule::Kind::USER);
    if (user_rule != group.is_account)
        return false;

    // SIDs are compared case-insensitively ("s-1-5-..." and "S-1-5-..."), GIDs and "uid:N" are
    // plain numbers.
    return rule_id.compare(group.id, Qt::CaseInsensitive) == 0;
}

//--------------------------------------------------------------------------------------------------
// static
QVector<DomainGroupRule> HostDomainAuthenticator::resolvedRules(
    const QVector<DomainGroupRule>& rules, const Resolver& resolver)
{
    QVector<DomainGroupRule> result;
    result.reserve(rules.size());

    for (const DomainGroupRule& rule : rules)
    {
        if (!rule.id.trimmed().isEmpty())
        {
            result.append(rule);
            continue;
        }

        if (rule.group.trimmed().isEmpty() || !resolver)
            continue;

        DomainAccountInfo info;
        QString error;
        if (!resolver(rule.group, &info, &error) || info.id.isEmpty())
        {
            LOG(WARNING) << "Domain group rule" << rule.group << "has no identifier and can not be"
                         << "resolved (" << error << "). The rule is ignored. Open the host settings"
                         << "and add the group again";
            continue;
        }

        // The rules are resolved for every connection; tell the administrator once per process.
        static QMutex warned_lock;
        static QSet<QString> warned;
        {
            QMutexLocker locker(&warned_lock);
            const QString warn_key = rule.group + QLatin1Char('|') + info.id;
            if (warned.contains(warn_key))
            {
                DomainGroupRule resolved = rule;
                resolved.id = info.id;
                resolved.kind = info.kind;
                result.append(resolved);
                continue;
            }
            warned.insert(warn_key);
        }

        LOG(WARNING) << "Domain group rule" << rule.group << "was saved without an identifier;"
                     << "resolved to" << info.id << "(" << info.name << "). Open the host settings"
                     << "and save them to store the identifier";

        DomainGroupRule resolved = rule;
        resolved.id = info.id;
        resolved.kind = info.kind;
        result.append(resolved);
    }

    for (const DomainGroupRule& rule : std::as_const(result))
    {
        static QMutex broad_lock;
        static QSet<QString> broad_warned;
        if (rule.kind == DomainGroupRule::Kind::GROUP && isBroadGroup(rule.id, rule.group))
        {
            QMutexLocker locker(&broad_lock);
            if (broad_warned.contains(rule.id))
                continue;
            broad_warned.insert(rule.id);

            LOG(WARNING) << "Domain group rule" << rule.group << "(" << rule.id << ") maps a group"
                         << "that contains practically every account";
        }
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
bool isValidPamServiceName(const QString& name)
{
    if (name.isEmpty() || name.size() > 64 || name.startsWith(QLatin1Char('.')))
        return false;

    for (const QChar ch : name)
    {
        const char16_t c = ch.unicode();
        const bool valid = (c >= u'a' && c <= u'z') || (c >= u'A' && c <= u'Z') ||
                           (c >= u'0' && c <= u'9') || c == u'.' || c == u'_' || c == u'-';
        if (!valid)
            return false;
    }

    return true;
}
