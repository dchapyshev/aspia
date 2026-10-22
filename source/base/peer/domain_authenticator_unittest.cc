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

#include "base/peer/domain_authenticator.h"

#include <gtest/gtest.h>

#include "base/peer/host_key.h"
#include "base/peer/user.h"

// ============================================================================
// splitAccountName
// ============================================================================

TEST(domain_authenticator_test, split_down_level_name)
{
    QString domain;
    QString user;
    DomainAuthenticator::splitAccountName("CORP\\ivanov", &domain, &user);
    EXPECT_EQ(domain, "CORP");
    EXPECT_EQ(user, "ivanov");
}

TEST(domain_authenticator_test, split_upn_name)
{
    QString domain = "garbage";
    QString user;
    DomainAuthenticator::splitAccountName("ivanov@corp.example.com", &domain, &user);
    EXPECT_TRUE(domain.isEmpty());
    EXPECT_EQ(user, "ivanov@corp.example.com");
}

TEST(domain_authenticator_test, split_plain_name)
{
    QString domain;
    QString user;
    DomainAuthenticator::splitAccountName("ivanov", &domain, &user);
    EXPECT_TRUE(domain.isEmpty());
    EXPECT_EQ(user, "ivanov");
}

TEST(domain_authenticator_test, status_to_string)
{
    EXPECT_STREQ(DomainAuthenticator::statusToString(DomainAuthenticator::Status::SUCCESS), "SUCCESS");
    EXPECT_STREQ(DomainAuthenticator::statusToString(DomainAuthenticator::Status::ACCOUNT_LOCKED),
                 "ACCOUNT_LOCKED");
}

// ============================================================================
// User::isValidDomainUserName
// ============================================================================

TEST(domain_authenticator_test, valid_domain_user_names)
{
    EXPECT_TRUE(User::isValidDomainUserName("ivanov"));
    EXPECT_TRUE(User::isValidDomainUserName("CORP\\ivanov"));
    EXPECT_TRUE(User::isValidDomainUserName("ivanov@corp.example.com"));
    EXPECT_TRUE(User::isValidDomainUserName("CORP\\Ivan Ivanov"));
    EXPECT_TRUE(User::isValidDomainUserName("corp.example.com\\i.ivanov"));
    EXPECT_TRUE(User::isValidDomainUserName(QString::fromUtf8("CORP\\иванов")));
}

TEST(domain_authenticator_test, invalid_domain_user_names)
{
    EXPECT_FALSE(User::isValidDomainUserName(""));
    EXPECT_FALSE(User::isValidDomainUserName("\\ivanov"));
    EXPECT_FALSE(User::isValidDomainUserName("CORP\\"));
    EXPECT_FALSE(User::isValidDomainUserName("A\\B\\C"));
    EXPECT_FALSE(User::isValidDomainUserName("@corp"));
    EXPECT_FALSE(User::isValidDomainUserName("ivanov@"));
    EXPECT_FALSE(User::isValidDomainUserName("a@b@c"));
    EXPECT_FALSE(User::isValidDomainUserName("CORP\\ivanov@corp"));
    EXPECT_FALSE(User::isValidDomainUserName(" ivanov"));
    EXPECT_FALSE(User::isValidDomainUserName("ivanov "));
    EXPECT_FALSE(User::isValidDomainUserName("iva/nov"));
    EXPECT_FALSE(User::isValidDomainUserName(QString(User::kMaxDomainUserNameLength + 1, 'a')));
    EXPECT_FALSE(User::isValidDomainUserName(QString::fromUtf16(u"iva\u0000nov", 7)));
}

TEST(domain_authenticator_test, computer_accounts_are_refused)
{
    EXPECT_FALSE(User::isValidDomainUserName("PC01$"));
    EXPECT_FALSE(User::isValidDomainUserName("CORP\\PC01$"));
    EXPECT_FALSE(User::isValidDomainUserName("PC01$@corp.example.com"));
    EXPECT_FALSE(User::isValidDomainUserName("CORP\\$"));

    // A dollar sign inside the name is still allowed (Active Directory allows it).
    EXPECT_TRUE(User::isValidDomainUserName("CORP\\svc$backup"));
    EXPECT_TRUE(User::isValidDomainUserName("svc$backup@corp.example.com"));
}

TEST(domain_authenticator_test, credential_text)
{
    EXPECT_TRUE(User::isValidCredentialText(""));
    EXPECT_TRUE(User::isValidCredentialText("ivanov"));
    EXPECT_TRUE(User::isValidCredentialText("\xd0\xb8\xd0\xb2\xd0\xb0\xd0\xbd")); // "иван"
    EXPECT_TRUE(User::isValidCredentialText("\xf0\x9f\x94\x91"));                 // U+1F511

    EXPECT_FALSE(User::isValidCredentialText(std::string("pass\0word", 9)));
    EXPECT_FALSE(User::isValidCredentialText(std::string(1, '\0')));
    EXPECT_FALSE(User::isValidCredentialText("\xff"));                 // never valid in UTF-8
    EXPECT_FALSE(User::isValidCredentialText("\xd0"));                 // truncated sequence
    EXPECT_FALSE(User::isValidCredentialText("\xc0\xaf"));             // overlong "/"
    EXPECT_FALSE(User::isValidCredentialText("\xed\xa0\x80"));         // UTF-16 surrogate
    EXPECT_FALSE(User::isValidCredentialText("\xf4\x90\x80\x80"));     // above U+10FFFF
}

TEST(domain_authenticator_test, srp_user_names_unchanged)
{
    // The SRP user name rules are not relaxed by the domain mode.
    EXPECT_FALSE(User::isValidUserName("CORP\\ivanov"));
    EXPECT_TRUE(User::isValidUserName("ivanov@corp"));
}

// ============================================================================
// HostKey::fingerprint
// ============================================================================

TEST(domain_authenticator_test, host_key_fingerprint)
{
    const QByteArray key(HostKey::kPublicKeySize, '\x01');
    const QString fingerprint = HostKey::fingerprint(key);

    // 64 hex characters in 16 groups of 4 separated by 15 spaces.
    EXPECT_EQ(fingerprint.size(), 64 + 15);
    EXPECT_EQ(fingerprint, HostKey::fingerprint(key));
    EXPECT_NE(fingerprint, HostKey::fingerprint(QByteArray(HostKey::kPublicKeySize, '\x02')));
    EXPECT_EQ(fingerprint, fingerprint.toUpper());
}

TEST(domain_authenticator_test, host_key_fingerprint_invalid)
{
    EXPECT_TRUE(HostKey::fingerprint(QByteArray()).isEmpty());
    EXPECT_TRUE(HostKey::fingerprint(QByteArray(31, 'a')).isEmpty());
}
