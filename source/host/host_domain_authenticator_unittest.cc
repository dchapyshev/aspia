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
#include "host/domain_logon.h"

#include <gtest/gtest.h>

#include "proto/peer.h"

namespace {

DomainGroupRule rule(const QString& id, quint32 sessions,
                     DomainGroupRule::Kind kind = DomainGroupRule::Kind::GROUP,
                     const QString& name = QString())
{
    DomainGroupRule result;
    result.id = id;
    result.group = name.isEmpty() ? id : name;
    result.kind = kind;
    result.sessions = sessions;
    return result;
}

DomainGroupRule legacyRule(const QString& group, quint32 sessions)
{
    DomainGroupRule result;
    result.group = group;
    result.sessions = sessions;
    return result;
}

DomainLogonGroup group(const QString& id)
{
    DomainLogonGroup result;
    result.id = id;
    return result;
}

DomainLogonGroup account(const QString& id)
{
    DomainLogonGroup result;
    result.id = id;
    result.is_account = true;
    return result;
}

const quint32 kDesktop = proto::peer::SESSION_TYPE_DESKTOP;
const quint32 kFiles = proto::peer::SESSION_TYPE_FILE_TRANSFER;
const quint32 kInfo = proto::peer::SESSION_TYPE_SYSTEM_INFO;
const DomainGroupRule::Kind kUser = DomainGroupRule::Kind::USER;

const QString kOps = "S-1-5-21-1004336348-1177238915-682003330-1105";

} // namespace

TEST(host_domain_authenticator_test, match_by_sid)
{
    EXPECT_TRUE(HostDomainAuthenticator::isMatch(rule(kOps, kDesktop), group(kOps)));
    EXPECT_TRUE(HostDomainAuthenticator::isMatch(rule(kOps.toLower(), kDesktop), group(kOps)));
    EXPECT_FALSE(HostDomainAuthenticator::isMatch(rule(kOps, kDesktop), group("S-1-5-32-544")));
    EXPECT_FALSE(HostDomainAuthenticator::isMatch(rule(kOps, kDesktop), group(kOps + "0")));
}

TEST(host_domain_authenticator_test, match_by_gid_and_uid)
{
    EXPECT_TRUE(HostDomainAuthenticator::isMatch(rule("1500", kDesktop), group("1500")));
    EXPECT_FALSE(HostDomainAuthenticator::isMatch(rule("1500", kDesktop), group("15000")));
    EXPECT_TRUE(HostDomainAuthenticator::isMatch(rule("uid:1001", kDesktop, kUser), account("uid:1001")));
    EXPECT_FALSE(HostDomainAuthenticator::isMatch(rule("uid:1001", kDesktop, kUser), group("1001")));
    EXPECT_FALSE(HostDomainAuthenticator::isMatch(rule("uid:1001", kDesktop, kUser), account("uid:1002")));
}

TEST(host_domain_authenticator_test, names_are_never_compared)
{
    // The rule was saved for "CORP\Aspia Operators"; a group or a user with this name in another
    // domain has another SID.
    const DomainGroupRule ops = rule(kOps, kDesktop, DomainGroupRule::Kind::GROUP,
                                     "CORP\\Aspia Operators");
    EXPECT_FALSE(HostDomainAuthenticator::isMatch(ops, group("S-1-5-21-9-9-9-1105")));
    EXPECT_FALSE(HostDomainAuthenticator::isMatch(ops, account("S-1-5-21-9-9-9-1106")));
}

TEST(host_domain_authenticator_test, group_rule_never_matches_the_account_itself)
{
    // Even with the same identifier (a group rule must not grant access to the account whose own
    // SID / uid happens to be listed as a group).
    EXPECT_FALSE(HostDomainAuthenticator::isMatch(rule(kOps, kDesktop), account(kOps)));
    EXPECT_TRUE(HostDomainAuthenticator::isMatch(rule(kOps, kDesktop, kUser), account(kOps)));
    EXPECT_FALSE(HostDomainAuthenticator::isMatch(rule(kOps, kDesktop, kUser), group(kOps)));
}

TEST(host_domain_authenticator_test, rule_without_id_never_matches)
{
    EXPECT_FALSE(HostDomainAuthenticator::isMatch(legacyRule("Ops", kDesktop), group("1")));
    EXPECT_FALSE(HostDomainAuthenticator::isMatch(rule("", kDesktop), group("")));
    EXPECT_FALSE(HostDomainAuthenticator::isMatch(rule("1", kDesktop), group("")));
}

TEST(host_domain_authenticator_test, sessions_are_combined)
{
    const QVector<DomainGroupRule> rules =
    {
        rule("S-1-5-21-2", kInfo),
        rule("S-1-5-21-3", kDesktop | kFiles),
        rule("S-1-5-21-4", proto::peer::SESSION_TYPE_TERMINAL)
    };

    const QVector<DomainLogonGroup> groups =
    {
        account("S-1-5-21-9"),
        group("S-1-5-21-1"),
        group("S-1-5-21-2"),
        group("S-1-5-21-3")
    };

    EXPECT_EQ(HostDomainAuthenticator::sessionsForGroups(rules, groups), kInfo | kDesktop | kFiles);
}

TEST(host_domain_authenticator_test, no_matching_groups)
{
    const QVector<DomainGroupRule> rules = { rule("S-1-5-21-3", kDesktop) };
    const QVector<DomainLogonGroup> groups = { group("S-1-5-21-1") };

    EXPECT_EQ(HostDomainAuthenticator::sessionsForGroups(rules, groups), 0u);
    EXPECT_EQ(HostDomainAuthenticator::sessionsForGroups({}, groups), 0u);
}

TEST(host_domain_authenticator_test, unknown_session_bits_are_dropped)
{
    const QVector<DomainGroupRule> rules = { rule("1", 0xFFFFFFFF) };
    const QVector<DomainLogonGroup> groups = { group("1") };

    EXPECT_EQ(HostDomainAuthenticator::sessionsForGroups(rules, groups),
              static_cast<quint32>(proto::peer::SESSION_TYPE_ALL));
}

TEST(host_domain_authenticator_test, broad_groups)
{
    using A = HostDomainAuthenticator;

    EXPECT_TRUE(A::isBroadGroup("S-1-1-0", "Everyone"));
    EXPECT_TRUE(A::isBroadGroup("s-1-5-11", QString()));
    EXPECT_TRUE(A::isBroadGroup("S-1-5-2", QString()));
    EXPECT_TRUE(A::isBroadGroup("S-1-5-4", QString()));
    EXPECT_TRUE(A::isBroadGroup("S-1-5-32-545", "BUILTIN\\Users"));
    EXPECT_TRUE(A::isBroadGroup("S-1-5-32-546", QString()));
    EXPECT_TRUE(A::isBroadGroup("S-1-5-21-1004336348-1177238915-682003330-513", "CORP\\Domain Users"));
    EXPECT_TRUE(A::isBroadGroup("S-1-5-21-1004336348-1177238915-682003330-514", QString()));
    EXPECT_TRUE(A::isBroadGroup("100", "users"));
    EXPECT_TRUE(A::isBroadGroup("1234", "users"));
    EXPECT_TRUE(A::isBroadGroup("1879000513", "domain users@corp.example.com"));
    EXPECT_TRUE(A::isBroadGroup("1879000513", "CORP\\Domain Users"));

    EXPECT_FALSE(A::isBroadGroup("S-1-5-21-1004336348-1177238915-682003330-1105", "CORP\\Aspia Operators"));
    EXPECT_FALSE(A::isBroadGroup("S-1-5-21-1004336348-1177238915-682003330-512", "CORP\\Domain Admins"));
    EXPECT_FALSE(A::isBroadGroup("S-1-5-32-544", "BUILTIN\\Administrators"));
    EXPECT_FALSE(A::isBroadGroup("S-1-5-21-1-2-3-1513", QString()));
    EXPECT_FALSE(A::isBroadGroup("1500", "aspia-operators"));
    EXPECT_FALSE(A::isBroadGroup("1000", "power users"));
}

TEST(host_domain_authenticator_test, legacy_rules_are_resolved)
{
    int calls = 0;
    auto resolver = [&calls](const QString& text, DomainAccountInfo* info, QString* error)
    {
        ++calls;
        if (text == QLatin1String("aspia-ops"))
        {
            info->id = "1500";
            info->name = "aspia-ops";
            return true;
        }
        if (text == QLatin1String("uid:1001"))
        {
            info->id = "uid:1001";
            info->kind = DomainGroupRule::Kind::USER;
            return true;
        }
        *error = "not found";
        return false;
    };

    const QVector<DomainGroupRule> rules =
    {
        legacyRule("aspia-ops", kDesktop),
        legacyRule("uid:1001", kFiles),
        legacyRule("missing", kInfo),
        rule("1600", kInfo)
    };

    const QVector<DomainGroupRule> resolved = HostDomainAuthenticator::resolvedRules(rules, resolver);
    EXPECT_EQ(calls, 3); // Rules with an identifier are not resolved again.
    ASSERT_EQ(resolved.size(), 3);
    EXPECT_EQ(resolved[0].id, "1500");
    EXPECT_EQ(resolved[0].kind, DomainGroupRule::Kind::GROUP);
    EXPECT_EQ(resolved[1].id, "uid:1001");
    EXPECT_EQ(resolved[1].kind, kUser);
    EXPECT_EQ(resolved[2].id, "1600");

    EXPECT_EQ(HostDomainAuthenticator::sessionsForGroups(
        resolved, { account("uid:1001"), group("1500") }), kDesktop | kFiles);
}

#if defined(Q_OS_LINUX)
TEST(host_domain_authenticator_test, linux_resolver)
{
    DomainAccountInfo info;
    QString error;

    ASSERT_TRUE(resolveDomainAccount("root", &info, &error)) << error.toStdString();
    EXPECT_EQ(info.id, "0");
    EXPECT_EQ(info.kind, DomainGroupRule::Kind::GROUP);

    ASSERT_TRUE(resolveDomainAccount("0", &info, &error));
    EXPECT_EQ(info.id, "0");
    EXPECT_EQ(info.name, "root");

    ASSERT_TRUE(resolveDomainAccount("UID:0", &info, &error));
    EXPECT_EQ(info.id, "uid:0");
    EXPECT_EQ(info.kind, DomainGroupRule::Kind::USER);
    EXPECT_EQ(info.name, "root");

    EXPECT_FALSE(resolveDomainAccount("aspia-no-such-group-7f3a", &info, &error));
    EXPECT_FALSE(error.isEmpty());
    EXPECT_FALSE(resolveDomainAccount("uid:abc", &info, &error));
    EXPECT_FALSE(resolveDomainAccount("uid:+1", &info, &error));
    EXPECT_FALSE(resolveDomainAccount("", &info, &error));
}
#endif // defined(Q_OS_LINUX)

TEST(host_domain_authenticator_test, more_broad_groups)
{
    using A = HostDomainAuthenticator;
    for (const char* sid : { "S-1-5-15", "S-1-5-1000", "S-1-5-113", "S-1-5-114", "S-1-5-4",
                             "S-1-5-2", "S-1-5-32-546", "S-1-2-0", "S-1-5-32-555",
                             "S-1-5-21-1-2-3-514", "S-1-5-21-1-2-3-515", "S-1-5-21-1-2-3-501" })
    {
        EXPECT_TRUE(A::isBroadGroup(sid, QString())) << sid;
    }
    EXPECT_TRUE(A::isBroadGroup("12345", "CORP\\Domain Computers"));
    EXPECT_TRUE(A::isBroadGroup("65534", QString()));
    EXPECT_FALSE(A::isBroadGroup("S-1-5-21-1-2-3-1105", "CORP\\Aspia Operators"));
    EXPECT_FALSE(A::isBroadGroup("S-1-5-21-1-2-3-512", QString())); // Domain Admins: narrow.
}

TEST(host_domain_authenticator_test, pam_service_name)
{
    EXPECT_TRUE(isValidPamServiceName("aspia-domain"));
    EXPECT_TRUE(isValidPamServiceName("a.b_c-1"));
    EXPECT_FALSE(isValidPamServiceName(""));
    EXPECT_FALSE(isValidPamServiceName(".x"));
    EXPECT_FALSE(isValidPamServiceName("../shadow"));
    EXPECT_FALSE(isValidPamServiceName("a b"));
    EXPECT_FALSE(isValidPamServiceName(QString(65, 'a')));
    EXPECT_FALSE(isValidPamServiceName(QString::fromUtf8("сервис")));
}
