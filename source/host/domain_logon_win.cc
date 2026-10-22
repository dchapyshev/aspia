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

#include "host/domain_logon.h"

#include <qt_windows.h>
#include <lm.h>
#include <ntsecapi.h>
#include <cstring>
#include <sddl.h>

#include <iterator>
#include <memory>

#include "base/logging.h"
#include "base/crypto/secure_string.h"
#include "base/win/scoped_object.h"

namespace {

//--------------------------------------------------------------------------------------------------
DomainAuthenticator::Status statusFromError(DWORD error)
{
    switch (error)
    {
        case ERROR_LOGON_FAILURE:
        case ERROR_NO_SUCH_USER:
        case ERROR_WRONG_PASSWORD:
            return DomainAuthenticator::Status::INVALID_CREDENTIALS;

        case ERROR_ACCOUNT_DISABLED:
            return DomainAuthenticator::Status::ACCOUNT_DISABLED;

        case ERROR_ACCOUNT_LOCKED_OUT:
            return DomainAuthenticator::Status::ACCOUNT_LOCKED;

        case ERROR_ACCOUNT_EXPIRED:
            return DomainAuthenticator::Status::ACCOUNT_EXPIRED;

        case ERROR_PASSWORD_EXPIRED:
        case ERROR_PASSWORD_MUST_CHANGE:
            return DomainAuthenticator::Status::CREDENTIALS_EXPIRED;

        case ERROR_LOGON_TYPE_NOT_GRANTED:
        case ERROR_INVALID_LOGON_HOURS:
        case ERROR_INVALID_WORKSTATION:
        case ERROR_ACCOUNT_RESTRICTION:
            return DomainAuthenticator::Status::LOGON_DENIED;

        default:
            return DomainAuthenticator::Status::INTERNAL_ERROR;
    }
}

//--------------------------------------------------------------------------------------------------
// NetBIOS name of the domain the computer is joined to, or "." (local account database) if the
// computer is not joined.
QString joinedDomainName()
{
    LPWSTR name = nullptr;
    NETSETUP_JOIN_STATUS status = NetSetupUnknownStatus;

    if (NetGetJoinInformation(nullptr, &name, &status) != NERR_Success)
        return QStringLiteral(".");

    QString result;
    if (status == NetSetupDomainName && name)
        result = QString::fromWCharArray(name);

    if (name)
        NetApiBufferFree(name);

    return result.isEmpty() ? QStringLiteral(".") : result;
}

//--------------------------------------------------------------------------------------------------
QString sidToString(PSID sid)
{
    LPWSTR string_sid = nullptr;
    if (!ConvertSidToStringSidW(sid, &string_sid))
        return QString();

    QString result = QString::fromWCharArray(string_sid);
    LocalFree(string_sid);
    return result;
}

//--------------------------------------------------------------------------------------------------
// Returns "DOMAIN\name" for a SID, or an empty string if it can not be resolved.
QString sidToAccountName(PSID sid, SID_NAME_USE* use_out = nullptr)
{
    wchar_t name[256] = { 0 };
    wchar_t domain[256] = { 0 };
    DWORD name_size = static_cast<DWORD>(std::size(name));
    DWORD domain_size = static_cast<DWORD>(std::size(domain));
    SID_NAME_USE use = SidTypeUnknown;

    if (!LookupAccountSidW(nullptr, sid, name, &name_size, domain, &domain_size, &use))
        return QString();

    if (use_out)
        *use_out = use;

    const QString name_string = QString::fromWCharArray(name);
    const QString domain_string = QString::fromWCharArray(domain);

    if (domain_string.isEmpty())
        return name_string;

    return domain_string + QLatin1Char('\\') + name_string;
}

//--------------------------------------------------------------------------------------------------
std::unique_ptr<quint8[]> tokenInformation(HANDLE token, TOKEN_INFORMATION_CLASS info_class)
{
    DWORD size = 0;
    GetTokenInformation(token, info_class, nullptr, 0, &size);
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || !size)
    {
        PLOG(ERROR) << "GetTokenInformation failed";
        return nullptr;
    }

    std::unique_ptr<quint8[]> buffer = std::make_unique<quint8[]>(size);
    if (!GetTokenInformation(token, info_class, buffer.get(), size, &size))
    {
        PLOG(ERROR) << "GetTokenInformation failed";
        return nullptr;
    }

    return buffer;
}

//--------------------------------------------------------------------------------------------------
QString computerName()
{
    wchar_t name[MAX_COMPUTERNAME_LENGTH + 1] = { 0 };
    DWORD size = static_cast<DWORD>(std::size(name));
    if (!GetComputerNameW(name, &size))
        return QString();
    return QString::fromWCharArray(name, static_cast<int>(size));
}

//--------------------------------------------------------------------------------------------------
// Domains a name without a domain may belong to: this computer, its built-in and well-known groups
// and the domain the computer is joined to. Never a trusted domain.
bool isOwnDomain(const QString& domain)
{
    if (domain.isEmpty())
        return true; // Well-known groups such as "Everyone".

    if (domain.compare(QLatin1String("BUILTIN"), Qt::CaseInsensitive) == 0 ||
        domain.compare(QLatin1String("NT AUTHORITY"), Qt::CaseInsensitive) == 0)
    {
        return true;
    }

    const QString computer = computerName();
    if (!computer.isEmpty() && domain.compare(computer, Qt::CaseInsensitive) == 0)
        return true;

    const QString joined = joinedDomainName();
    return joined != QLatin1String(".") && domain.compare(joined, Qt::CaseInsensitive) == 0;
}

//--------------------------------------------------------------------------------------------------
bool kindFromUse(SID_NAME_USE use, DomainGroupRule::Kind* kind)
{
    switch (use)
    {
        case SidTypeUser:
            *kind = DomainGroupRule::Kind::USER;
            return true;

        case SidTypeGroup:
        case SidTypeAlias:
        case SidTypeWellKnownGroup:
            *kind = DomainGroupRule::Kind::GROUP;
            return true;

        default:
            // Computer accounts, domains, deleted accounts, labels etc. never grant anything.
            return false;
    }
}

// True if |account| is an account of the domain |domain| (same authority and sub-authorities plus
// exactly one RID).
bool isAccountOfDomain(PSID account, PSID domain)
{
    if (!account || !domain || !IsValidSid(account) || !IsValidSid(domain))
        return false;

    const UCHAR account_count = *GetSidSubAuthorityCount(account);
    const UCHAR domain_count = *GetSidSubAuthorityCount(domain);
    if (account_count != domain_count + 1)
        return false;

    if (memcmp(GetSidIdentifierAuthority(account), GetSidIdentifierAuthority(domain),
               sizeof(SID_IDENTIFIER_AUTHORITY)) != 0)
    {
        return false;
    }

    for (DWORD i = 0; i < domain_count; ++i)
    {
        if (*GetSidSubAuthority(account, i) != *GetSidSubAuthority(domain, i))
            return false;
    }

    return true;
}

// True if |account| belongs to this computer (local account) or to the domain the computer is
// joined to. Accounts of trusted domains and forests are refused (a UPN logon could otherwise sign
// in a user of any trusted forest, who would then match broad well-known groups).
bool isOwnDomainAccount(PSID account)
{
    LSA_OBJECT_ATTRIBUTES attributes;
    memset(&attributes, 0, sizeof(attributes));

    LSA_HANDLE policy = nullptr;
    if (LsaOpenPolicy(nullptr, &attributes, POLICY_VIEW_LOCAL_INFORMATION, &policy) != 0)
    {
        LOG(ERROR) << "LsaOpenPolicy failed";
        return false;
    }

    bool result = false;

    PPOLICY_ACCOUNT_DOMAIN_INFO account_domain = nullptr;
    if (LsaQueryInformationPolicy(policy, PolicyAccountDomainInformation,
                                  reinterpret_cast<PVOID*>(&account_domain)) == 0 && account_domain)
    {
        result = isAccountOfDomain(account, account_domain->DomainSid);
        LsaFreeMemory(account_domain);
    }

    if (!result)
    {
        PPOLICY_PRIMARY_DOMAIN_INFO primary_domain = nullptr;
        if (LsaQueryInformationPolicy(policy, PolicyPrimaryDomainInformation,
                                      reinterpret_cast<PVOID*>(&primary_domain)) == 0 && primary_domain)
        {
            if (primary_domain->Sid)
                result = isAccountOfDomain(account, primary_domain->Sid);
            LsaFreeMemory(primary_domain);
        }
    }

    LsaClose(policy);
    return result;
}

// Domain Computers (515), Domain Controllers (516), Read-only Domain Controllers (521).
bool isComputerGroupSid(const QString& sid)
{
    if (!sid.startsWith(QLatin1String("S-1-5-21-"), Qt::CaseInsensitive))
        return false;

    return sid.endsWith(QLatin1String("-515")) || sid.endsWith(QLatin1String("-516")) ||
           sid.endsWith(QLatin1String("-521"));
}

} // namespace

//--------------------------------------------------------------------------------------------------
DomainLogonResult domainLogon(const QString& domain,
                              const QString& username,
                              const SecureString& password,
                              const QString& /* peer_address */,
                              const QString& /* pam_service */)
{
    DomainLogonResult result;

    // A UPN ("user@domain.tld") is passed with a null domain, as LogonUserW requires. A plain name
    // without a domain means the domain the computer is joined to.
    const bool is_upn = domain.isEmpty() && username.contains(QLatin1Char('@'));
    const QString logon_domain = is_upn ? QString() : (domain.isEmpty() ? joinedDomainName() : domain);

    ScopedHandle token;

    // LOGON32_LOGON_NETWORK: verifies the credentials without creating an interactive session and
    // without caching them on the computer. The account needs the "Access this computer from the
    // network" right (granted to Users by default).
    if (!LogonUserW(qUtf16Printable(username),
                    is_upn ? nullptr : qUtf16Printable(logon_domain),
                    reinterpret_cast<const wchar_t*>(password.toString().utf16()),
                    LOGON32_LOGON_NETWORK,
                    LOGON32_PROVIDER_DEFAULT,
                    token.recieve()))
    {
        const DWORD error = GetLastError();
        LOG(INFO) << "LogonUserW failed for" << username << "domain:" << logon_domain
                  << "error:" << error;

        result.status = statusFromError(error);
        return result;
    }

    // Canonical "DOMAIN\user" of the account that actually logged on. Every check below fails
    // closed: if the type of the account can not be established, the logon is refused.
    std::unique_ptr<quint8[]> user_buffer = tokenInformation(token.get(), TokenUser);
    if (!user_buffer)
    {
        result.status = DomainAuthenticator::Status::INTERNAL_ERROR;
        return result;
    }

    {
        const TOKEN_USER* token_user = reinterpret_cast<const TOKEN_USER*>(user_buffer.get());
        SID_NAME_USE use = SidTypeUnknown;
        result.account = sidToAccountName(token_user->User.Sid, &use);

        // Only user accounts sign in. A computer account (its password is known to the computer
        // itself and to anyone with administrative rights on it), a gMSA or an account whose type
        // can not be looked up right now (no domain controller) is refused, whatever name the
        // client typed.
        if (result.account.isEmpty() || use != SidTypeUser)
        {
            LOG(INFO) << "Account" << result.account << "of type" << static_cast<int>(use)
                      << "refused (not a user account or unknown type)";
            result.account.clear();
            result.status = (use == SidTypeComputer) ?
                DomainAuthenticator::Status::INVALID_CREDENTIALS :
                DomainAuthenticator::Status::INTERNAL_ERROR;
            return result;
        }

        if (!isOwnDomainAccount(token_user->User.Sid))
        {
            LOG(INFO) << "Account" << result.account << "is not an account of this computer or of "
                      << "the domain it is joined to - refused";
            result.status = DomainAuthenticator::Status::LOGON_DENIED;
            return result;
        }
    }

    // The account itself, for rules that name a single user by SID.
    if (user_buffer)
    {
        const TOKEN_USER* token_user = reinterpret_cast<const TOKEN_USER*>(user_buffer.get());

        DomainLogonGroup self;
        self.id = sidToString(token_user->User.Sid);
        self.is_account = true;

        if (!self.id.isEmpty())
            result.groups.append(self);
    }

    std::unique_ptr<quint8[]> groups_buffer = tokenInformation(token.get(), TokenGroups);
    if (!groups_buffer)
    {
        result.status = DomainAuthenticator::Status::INTERNAL_ERROR;
        return result;
    }

    const TOKEN_GROUPS* token_groups = reinterpret_cast<const TOKEN_GROUPS*>(groups_buffer.get());

    for (DWORD i = 0; i < token_groups->GroupCount; ++i)
    {
        const SID_AND_ATTRIBUTES& group = token_groups->Groups[i];

        // The logon SID is unique for the logon session, and deny-only groups must never grant
        // anything.
        if ((group.Attributes & SE_GROUP_LOGON_ID) == SE_GROUP_LOGON_ID)
            continue;
        if (group.Attributes & SE_GROUP_USE_FOR_DENY_ONLY)
            continue;
        if (!(group.Attributes & SE_GROUP_ENABLED))
            continue;

        // Rules are matched by SID, so the names of the groups are not looked up (each lookup may
        // go to a domain controller).
        DomainLogonGroup item;
        item.id = sidToString(group.Sid);
        if (item.id.isEmpty())
            continue;

        // Membership in Domain Computers / Domain Controllers means a machine account, even if
        // the lookup above reported a user (e.g. a gMSA in a custom configuration).
        if (isComputerGroupSid(item.id))
        {
            LOG(INFO) << "Account" << result.account << "is a member of a computer group - refused";
            result.groups.clear();
            result.status = DomainAuthenticator::Status::INVALID_CREDENTIALS;
            return result;
        }

        result.groups.append(item);
    }

    // The primary group of a computer account is Domain Computers; it is not always in TokenGroups.
    std::unique_ptr<quint8[]> primary_buffer = tokenInformation(token.get(), TokenPrimaryGroup);
    if (!primary_buffer)
    {
        result.groups.clear();
        result.status = DomainAuthenticator::Status::INTERNAL_ERROR;
        return result;
    }

    const TOKEN_PRIMARY_GROUP* primary_group =
        reinterpret_cast<const TOKEN_PRIMARY_GROUP*>(primary_buffer.get());
    if (isComputerGroupSid(sidToString(primary_group->PrimaryGroup)))
    {
        LOG(INFO) << "Account" << result.account << "has a computer primary group - refused";
        result.groups.clear();
        result.status = DomainAuthenticator::Status::INVALID_CREDENTIALS;
        return result;
    }

    // |token| is closed by ScopedHandle.
    result.status = DomainAuthenticator::Status::SUCCESS;
    return result;
}

//--------------------------------------------------------------------------------------------------
bool resolveDomainAccount(const QString& text, DomainAccountInfo* info, QString* error)
{
    if (!info)
        return false;

    const QString value = text.trimmed();
    *info = DomainAccountInfo();

    auto fail = [error](const QString& message)
    {
        if (error)
            *error = message;
        return false;
    };

    if (value.isEmpty())
        return fail(QStringLiteral("The group is empty"));

    if (value.startsWith(QLatin1String("S-1-"), Qt::CaseInsensitive))
    {
        PSID sid = nullptr;
        if (!ConvertStringSidToSidW(qUtf16Printable(value), &sid) || !sid)
            return fail(QStringLiteral("Invalid SID"));

        info->id = sidToString(sid);

        SID_NAME_USE use = SidTypeUnknown;
        info->name = sidToAccountName(sid, &use);
        LocalFree(sid);

        if (info->id.isEmpty())
            return fail(QStringLiteral("Invalid SID"));

        // A SID that can not be looked up right now (e.g. no domain controller) is kept as a group.
        if (!info->name.isEmpty() && !kindFromUse(use, &info->kind))
            return fail(QStringLiteral("The SID is not a user or a group"));

        return true;
    }

    // A name. LookupAccountNameW would also search trusted domains for a name without a domain;
    // such a result is refused, the administrator has to write the domain out.
    const bool has_domain = value.contains(QLatin1Char('\\')) || value.contains(QLatin1Char('@'));

    DWORD sid_size = 0;
    DWORD domain_size = 0;
    SID_NAME_USE use = SidTypeUnknown;

    LookupAccountNameW(nullptr, qUtf16Printable(value), nullptr, &sid_size, nullptr, &domain_size, &use);
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || !sid_size)
        return fail(QStringLiteral("Account \"%1\" was not found").arg(value));

    std::unique_ptr<quint8[]> sid_buffer = std::make_unique<quint8[]>(sid_size);
    std::unique_ptr<wchar_t[]> domain_buffer = std::make_unique<wchar_t[]>(domain_size + 1);

    if (!LookupAccountNameW(nullptr, qUtf16Printable(value), sid_buffer.get(), &sid_size,
                            domain_buffer.get(), &domain_size, &use))
    {
        return fail(QStringLiteral("Account \"%1\" was not found").arg(value));
    }

    const QString found_domain = QString::fromWCharArray(domain_buffer.get(), static_cast<int>(domain_size));

    if (!has_domain && !isOwnDomain(found_domain))
    {
        return fail(QStringLiteral("\"%1\" was found in the domain %2, not in the domain of this "
                                   "computer. Enter it as \"%2\\%1\".").arg(value, found_domain));
    }

    if (!kindFromUse(use, &info->kind))
        return fail(QStringLiteral("\"%1\" is not a user or a group").arg(value));

    PSID sid = reinterpret_cast<PSID>(sid_buffer.get());
    info->id = sidToString(sid);
    info->name = sidToAccountName(sid);
    if (info->name.isEmpty())
        info->name = found_domain.isEmpty() ? value : (found_domain + QLatin1Char('\\') + value);

    if (info->id.isEmpty())
        return fail(QStringLiteral("Unable to convert the SID"));

    return true;
}

//--------------------------------------------------------------------------------------------------
bool isUsablePamService(const QString& /* name */, QString* /* error */)
{
    // PAM is not used on this platform.
    return true;
}

//--------------------------------------------------------------------------------------------------
bool resolveLegacyDomainRule(const QString& text, DomainAccountInfo* info, QString* error)
{
    // The Windows resolver did not change: older rules resolve exactly as before.
    return resolveDomainAccount(text, info, error);
}
