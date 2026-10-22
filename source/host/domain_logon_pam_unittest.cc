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

#include "host/domain_logon_pam_internal.h"
#include "host/domain_logon.h"

#include <gtest/gtest.h>

#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <vector>

namespace {

using domain_logon_pam::PamConversationData;
using domain_logon_pam::pamConversation;

void freeAll(struct pam_response* resp, int count)
{
    if (!resp)
        return;
    for (int i = 0; i < count; ++i)
        free(resp[i].resp);
    free(resp);
}

int converse(PamConversationData* data, std::initializer_list<int> styles,
             struct pam_response** resp)
{
    std::vector<struct pam_message> messages;
    for (int style : styles)
        messages.push_back({ style, "prompt" });

    std::vector<const struct pam_message*> pointers;
    for (const auto& message : messages)
        pointers.push_back(&message);

    *resp = nullptr;
    return pamConversation(static_cast<int>(pointers.size()), pointers.data(), resp, data);
}

} // namespace

TEST(DomainLogonPamTest, FirstPasswordPromptIsAnswered)
{
    PamConversationData data;
    data.password = "secret";

    struct pam_response* resp = nullptr;
    ASSERT_EQ(converse(&data, { PAM_TEXT_INFO, PAM_PROMPT_ECHO_OFF }, &resp), PAM_SUCCESS);
    ASSERT_NE(resp, nullptr);
    EXPECT_EQ(resp[0].resp, nullptr);
    ASSERT_NE(resp[1].resp, nullptr);
    EXPECT_STREQ(resp[1].resp, "secret");
    EXPECT_TRUE(data.password_given);
    freeAll(resp, 2);
}

TEST(DomainLogonPamTest, SecondPasswordPromptFails)
{
    PamConversationData data;
    data.password = "secret";

    struct pam_response* resp = nullptr;
    ASSERT_EQ(converse(&data, { PAM_PROMPT_ECHO_OFF }, &resp), PAM_SUCCESS);
    freeAll(resp, 1);

    // A module asks again (OTP, new password): the password is not given twice.
    EXPECT_EQ(converse(&data, { PAM_PROMPT_ECHO_OFF }, &resp), PAM_CONV_ERR);
    EXPECT_EQ(resp, nullptr);

    // Two prompts in one call fail as well.
    PamConversationData fresh;
    fresh.password = "secret";
    EXPECT_EQ(converse(&fresh, { PAM_PROMPT_ECHO_OFF, PAM_PROMPT_ECHO_OFF }, &resp), PAM_CONV_ERR);
    EXPECT_EQ(resp, nullptr);
}

TEST(DomainLogonPamTest, EchoOnPromptFails)
{
    PamConversationData data;
    data.password = "secret";

    struct pam_response* resp = nullptr;
    EXPECT_EQ(converse(&data, { PAM_PROMPT_ECHO_ON }, &resp), PAM_CONV_ERR);
    EXPECT_EQ(resp, nullptr);
    EXPECT_FALSE(data.password_given);

    EXPECT_EQ(converse(&data, { PAM_PROMPT_ECHO_OFF, PAM_PROMPT_ECHO_ON }, &resp), PAM_CONV_ERR);
    EXPECT_EQ(resp, nullptr);
}

TEST(DomainLogonPamTest, InvalidArgumentsFail)
{
    PamConversationData data;
    struct pam_response* resp = nullptr;
    EXPECT_EQ(pamConversation(0, nullptr, &resp, &data), PAM_CONV_ERR);
    EXPECT_EQ(pamConversation(PAM_MAX_NUM_MSG + 1, nullptr, &resp, &data), PAM_CONV_ERR);

    const struct pam_message* null_message = nullptr;
    EXPECT_EQ(pamConversation(1, &null_message, &resp, &data), PAM_CONV_ERR);
}

TEST(DomainLogonPamTest, RemoteHost)
{
    using domain_logon_pam::remoteHost;

    EXPECT_EQ(remoteHost(QStringLiteral("192.0.2.10")), "192.0.2.10");
    EXPECT_EQ(remoteHost(QStringLiteral("::ffff:192.0.2.10")), "192.0.2.10");
    EXPECT_EQ(remoteHost(QStringLiteral("2001:db8::1")), "2001:db8::1");
    EXPECT_EQ(remoteHost(QStringLiteral("[2001:db8::1]")), "2001:db8::1");
    EXPECT_EQ(remoteHost(QStringLiteral("fe80::1%eth0")), "fe80::1");
    EXPECT_EQ(remoteHost(QString()), "");
    EXPECT_EQ(remoteHost(QStringLiteral("evil.example.com")), "");
    EXPECT_EQ(remoteHost(QStringLiteral("1.2.3.4 x")), "");
}

TEST(DomainLogonPamTest, PamServiceMustExist)
{
    QString error;
    EXPECT_FALSE(isUsablePamService("aspia-no-such-service-xyz", &error));
    EXPECT_FALSE(error.isEmpty());
    EXPECT_FALSE(isUsablePamService("../passwd", &error));
    // "other" is shipped with Linux-PAM on every distribution.
    EXPECT_TRUE(isUsablePamService("other", &error) || isUsablePamService("login", &error));
}

//--------------------------------------------------------------------------------------------------
TEST(DomainLogonPamTest, AccountNameCandidatesBackslash)
{
    const domain_logon_pam::AccountNameCandidates c =
        domain_logon_pam::accountNameCandidates(QStringLiteral("ASUTP.TOF\\CimMgr"));
    EXPECT_EQ(c.domain, QStringLiteral("ASUTP.TOF"));
    EXPECT_EQ(c.name, QStringLiteral("CimMgr"));
    EXPECT_EQ(c.spellings, QStringList({ "ASUTP.TOF\\CimMgr", "asutp.tof\\cimmgr", "CimMgr@ASUTP.TOF",
                                         "cimmgr@asutp.tof" }));
    EXPECT_EQ(c.short_names, QStringList({ "CimMgr", "cimmgr" }));
}

//--------------------------------------------------------------------------------------------------
TEST(DomainLogonPamTest, AccountNameCandidatesAt)
{
    const domain_logon_pam::AccountNameCandidates c =
        domain_logon_pam::accountNameCandidates(QStringLiteral("cimmgr@asutp.tof"));
    EXPECT_EQ(c.domain, QStringLiteral("asutp.tof"));
    EXPECT_EQ(c.name, QStringLiteral("cimmgr"));
    EXPECT_EQ(c.spellings, QStringList({ "cimmgr@asutp.tof", "asutp.tof\\cimmgr" }));
    EXPECT_EQ(c.short_names, QStringList({ "cimmgr" }));
}

//--------------------------------------------------------------------------------------------------
TEST(DomainLogonPamTest, AccountNameCandidatesShortAndCyrillic)
{
    const domain_logon_pam::AccountNameCandidates c = domain_logon_pam::accountNameCandidates(
        QString::fromUtf8("Администраторы Домена"));
    EXPECT_TRUE(c.domain.isEmpty());
    EXPECT_EQ(c.spellings, QStringList({ QString::fromUtf8("Администраторы Домена"),
                                         QString::fromUtf8("администраторы домена") }));
    EXPECT_TRUE(c.short_names.isEmpty());

    const domain_logon_pam::AccountNameCandidates d = domain_logon_pam::accountNameCandidates(
        QString::fromUtf8("АСУТП\\Операторы"));
    EXPECT_EQ(d.domain, QString::fromUtf8("АСУТП"));
    EXPECT_TRUE(d.spellings.contains(QString::fromUtf8("операторы@асутп")));
    EXPECT_EQ(d.short_names, QStringList({ QString::fromUtf8("Операторы"),
                                           QString::fromUtf8("операторы") }));
}

//--------------------------------------------------------------------------------------------------
TEST(DomainLogonPamTest, AccountNameCandidatesMalformed)
{
    // A separator at an edge is not a domain.
    EXPECT_TRUE(domain_logon_pam::accountNameCandidates(QStringLiteral("\\name")).domain.isEmpty());
    EXPECT_TRUE(domain_logon_pam::accountNameCandidates(QStringLiteral("name@")).domain.isEmpty());
    EXPECT_TRUE(domain_logon_pam::accountNameCandidates(QStringLiteral("@d")).domain.isEmpty());
}

//--------------------------------------------------------------------------------------------------
TEST(DomainLogonPamTest, ResolveUserByName)
{
    // "root" is both a user and its primary group of the same name (kept as the group); the
    // prefixes pick one explicitly.
    DomainAccountInfo info;
    QString error;
    ASSERT_TRUE(resolveDomainAccount(QStringLiteral("user:root"), &info, &error))
        << error.toStdString();
    EXPECT_EQ(info.id, QStringLiteral("uid:0"));
    EXPECT_EQ(info.kind, DomainGroupRule::Kind::USER);

    ASSERT_TRUE(resolveDomainAccount(QStringLiteral("group:ROOT"), &info, &error))
        << error.toStdString();
    EXPECT_EQ(info.id, QStringLiteral("0"));
    EXPECT_EQ(info.kind, DomainGroupRule::Kind::GROUP);

    // A local account is never taken for a domain one through the short-name fallback.
    EXPECT_FALSE(resolveDomainAccount(QStringLiteral("NOSUCHDOMAIN\\root"), &info, &error));
    EXPECT_TRUE(error.contains(QStringLiteral("getent")));
}

//--------------------------------------------------------------------------------------------------
TEST(DomainLogonPamTest, SssdDefaultDomainAliases)
{
    const QString conf = QStringLiteral(
        "[sssd]\n"
        "services = nss, pam\n"
        "domains = asutp.tof, other.example\n"
        "\n"
        "[domain/asutp.tof]\n"
        "ad_domain = asutp.tof\n"
        "krb5_realm = ASUTP.TOF\n"
        "id_provider = ad\n"
        "[domain/other.example]\n"
        "ad_domain = other.example\n");
    const QStringList aliases = domain_logon_pam::sssdDefaultDomainAliases(conf);
    EXPECT_TRUE(domain_logon_pam::isDefaultDomain(QStringLiteral("ASUTP.TOF"), aliases));
    EXPECT_TRUE(domain_logon_pam::isDefaultDomain(QStringLiteral("asutp"), aliases));
    EXPECT_TRUE(domain_logon_pam::isDefaultDomain(QStringLiteral("Asutp"), aliases));
    EXPECT_FALSE(domain_logon_pam::isDefaultDomain(QStringLiteral("other.example"), aliases));
    EXPECT_FALSE(domain_logon_pam::isDefaultDomain(QStringLiteral("other"), aliases));
    EXPECT_FALSE(domain_logon_pam::isDefaultDomain(QStringLiteral("PARTNER"), aliases));
    EXPECT_FALSE(domain_logon_pam::isDefaultDomain(QString(), aliases));

    // default_domain_suffix wins over the order of "domains"; conf.d appended later overrides.
    const QStringList suffix = domain_logon_pam::sssdDefaultDomainAliases(
        conf + QStringLiteral("\n[sssd]\ndefault_domain_suffix = other.example\n"));
    EXPECT_TRUE(domain_logon_pam::isDefaultDomain(QStringLiteral("OTHER"), suffix));
    EXPECT_FALSE(domain_logon_pam::isDefaultDomain(QStringLiteral("asutp.tof"), suffix));

    EXPECT_TRUE(domain_logon_pam::sssdDefaultDomainAliases(QString()).isEmpty());
    EXPECT_TRUE(domain_logon_pam::sssdDefaultDomainAliases(QStringLiteral("[sssd]\n")).isEmpty());
}

//--------------------------------------------------------------------------------------------------
TEST(DomainLogonPamTest, SambaDefaultDomainAliasesAndCyrillic)
{
    const QStringList aliases = domain_logon_pam::sambaDefaultDomainAliases(QStringLiteral(
        "[global]\n   workgroup = ASUTP\n   realm = ASUTP.TOF\n   security = ads\n[homes]\nrealm = X\n"));
    EXPECT_TRUE(domain_logon_pam::isDefaultDomain(QStringLiteral("asutp"), aliases));
    EXPECT_TRUE(domain_logon_pam::isDefaultDomain(QStringLiteral("asutp.tof"), aliases));
    EXPECT_FALSE(domain_logon_pam::isDefaultDomain(QStringLiteral("X"), aliases));

    EXPECT_TRUE(domain_logon_pam::isDefaultDomain(QString::fromUtf8("АСУТП"),
                                                  { QString::fromUtf8("асутп") }));
}

//--------------------------------------------------------------------------------------------------
TEST(DomainLogonPamTest, DomainOfNameAndLocalIdRange)
{
    EXPECT_EQ(domain_logon_pam::domainOfName(QStringLiteral("ASUTP\\cimmgr")), QStringLiteral("ASUTP"));
    EXPECT_EQ(domain_logon_pam::domainOfName(QStringLiteral("cimmgr@asutp.tof")), QStringLiteral("asutp.tof"));
    EXPECT_TRUE(domain_logon_pam::domainOfName(QStringLiteral("cimmgr")).isEmpty());

    EXPECT_TRUE(domain_logon_pam::isLocalIdRange(0, 1000));
    EXPECT_TRUE(domain_logon_pam::isLocalIdRange(999, 1000));
    EXPECT_FALSE(domain_logon_pam::isLocalIdRange(1000, 1000));
    EXPECT_FALSE(domain_logon_pam::isLocalIdRange(1625000512u, 1000));
    EXPECT_TRUE(domain_logon_pam::isLocalIdRange(65534, 1000));
    EXPECT_TRUE(domain_logon_pam::isLocalIdRange(61184, 1000));
    EXPECT_TRUE(domain_logon_pam::isLocalIdRange(65519, 1000));
    EXPECT_FALSE(domain_logon_pam::isLocalIdRange(65520, 1000));
}

//--------------------------------------------------------------------------------------------------
TEST(DomainLogonPamTest, UnknownDomainIsNotDroppedAndErrorHasNoShellCommand)
{
    // No sssd / samba on the test machine: a domain-qualified name that NSS does not know never
    // falls back to the bare name.
    DomainAccountInfo info;
    QString error;
    EXPECT_FALSE(resolveDomainAccount(QStringLiteral("PARTNER\\root"), &info, &error));
    EXPECT_TRUE(error.contains(QStringLiteral("not the domain this computer is joined to")));

    EXPECT_FALSE(resolveDomainAccount(QStringLiteral("x'; rm -rf ~; '"), &info, &error));
    EXPECT_FALSE(error.contains(QStringLiteral("getent group '")));
}

//--------------------------------------------------------------------------------------------------
TEST(DomainLogonPamTest, LegacyRuleKeepsGroupOnlySemantics)
{
    DomainAccountInfo info;
    QString error;

    // A group name as typed.
    ASSERT_TRUE(resolveLegacyDomainRule(QStringLiteral("root"), &info, &error));
    EXPECT_EQ(info.id, QStringLiteral("0"));
    EXPECT_EQ(info.kind, DomainGroupRule::Kind::GROUP);

    // GID and uid: as before.
    ASSERT_TRUE(resolveLegacyDomainRule(QStringLiteral("0"), &info, &error));
    EXPECT_EQ(info.id, QStringLiteral("0"));
    ASSERT_TRUE(resolveLegacyDomainRule(QStringLiteral("uid:0"), &info, &error));
    EXPECT_EQ(info.id, QStringLiteral("uid:0"));

    // Not resolved as a user, no other spellings, no prefixes.
    EXPECT_FALSE(resolveLegacyDomainRule(QStringLiteral("nobody"), &info, &error));
    EXPECT_FALSE(resolveLegacyDomainRule(QStringLiteral("ROOT"), &info, &error));
    EXPECT_FALSE(resolveLegacyDomainRule(QStringLiteral("user:root"), &info, &error));
    EXPECT_FALSE(resolveLegacyDomainRule(QString(), &info, &error));
}
