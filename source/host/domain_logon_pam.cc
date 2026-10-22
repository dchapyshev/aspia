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
#include "host/domain_logon_pam_internal.h"

#include <grp.h>
#include <pwd.h>
#include <security/pam_appl.h>
#include <unistd.h>

#include <QDir>
#include <QFile>
#include <QPair>

#include <cerrno>
#include <cstdio>
#include <strings.h>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/crypto/secure_memory.h"
#include "base/crypto/secure_string.h"

namespace {

const int kMaxGroups = 4096;

} // namespace

namespace domain_logon_pam {

//--------------------------------------------------------------------------------------------------
static void freeResponses(struct pam_response* responses, int count)
{
    for (int i = 0; i < count; ++i)
    {
        if (responses[i].resp)
        {
            memZero(responses[i].resp, strlen(responses[i].resp));
            free(responses[i].resp);
        }
    }
    free(responses);
}

//--------------------------------------------------------------------------------------------------
// Answers the first password prompt (PAM_PROMPT_ECHO_OFF) of the PAM stack with the password of
// the client, once per logon. Any other prompt fails the conversation: a second password prompt
// (an OTP, a new password for an expired account, a module that asks again) or an echo-on prompt
// (user name, OTP, "press Enter") must not be answered with the password, which could otherwise
// leak to a different factor or let a module treat it as something else.
int pamConversation(int num_msg, const struct pam_message** msg, struct pam_response** resp,
                    void* appdata_ptr)
{
    if (num_msg <= 0 || num_msg > PAM_MAX_NUM_MSG || !msg || !resp || !appdata_ptr)
        return PAM_CONV_ERR;

    auto* data = reinterpret_cast<PamConversationData*>(appdata_ptr);

    auto* responses = reinterpret_cast<struct pam_response*>(
        calloc(static_cast<size_t>(num_msg), sizeof(struct pam_response)));
    if (!responses)
        return PAM_BUF_ERR;

    for (int i = 0; i < num_msg; ++i)
    {
        if (!msg[i])
        {
            freeResponses(responses, num_msg);
            return PAM_CONV_ERR;
        }

        switch (msg[i]->msg_style)
        {
            case PAM_PROMPT_ECHO_OFF:
            {
                if (data->password_given)
                {
                    LOG(INFO) << "PAM asks for more than one secret (2FA or password change?):"
                              << (msg[i]->msg ? msg[i]->msg : "") << "- refused";
                    freeResponses(responses, num_msg);
                    return PAM_CONV_ERR;
                }

                // PAM frees the response with free(); it is the only copy outside of |data|.
                responses[i].resp = strdup(data->password.c_str());
                if (!responses[i].resp)
                {
                    freeResponses(responses, num_msg);
                    return PAM_BUF_ERR;
                }

                data->password_given = true;
            }
            break;

            case PAM_ERROR_MSG:
            case PAM_TEXT_INFO:
                LOG(INFO) << "PAM:" << (msg[i]->msg ? msg[i]->msg : "");
                break;

            default:
                // PAM_PROMPT_ECHO_ON (user name, OTP etc.) and unknown styles.
                LOG(INFO) << "PAM asks a question that can not be answered:"
                          << (msg[i]->msg ? msg[i]->msg : "") << "- refused";
                freeResponses(responses, num_msg);
                return PAM_CONV_ERR;
        }
    }

    *resp = responses;
    return PAM_SUCCESS;
}

//--------------------------------------------------------------------------------------------------
// PAM_RHOST value for |address|: brackets and the IPv6 zone are removed, an IPv4-mapped IPv6 address
// is written as IPv4 (as pam_faillock, pam_access and the audit log of the system expect). Anything
// that does not look like an IP address gives an empty string.
std::string remoteHost(const QString& address)
{
    QString host = address.trimmed();
    if (host.startsWith(QLatin1Char('[')) && host.endsWith(QLatin1Char(']')))
        host = host.mid(1, host.size() - 2);

    const qsizetype zone = host.indexOf(QLatin1Char('%'));
    if (zone >= 0)
        host.truncate(zone);

    if (host.startsWith(QLatin1String("::ffff:"), Qt::CaseInsensitive) && host.contains(QLatin1Char('.')))
        host = host.mid(7);

    if (host.isEmpty() || host.size() > 45)
        return std::string();

    for (const QChar ch : std::as_const(host))
    {
        const bool valid = (ch >= QLatin1Char('0') && ch <= QLatin1Char('9')) ||
                           (ch >= QLatin1Char('a') && ch <= QLatin1Char('f')) ||
                           (ch >= QLatin1Char('A') && ch <= QLatin1Char('F')) ||
                           ch == QLatin1Char(':') || ch == QLatin1Char('.');
        if (!valid)
            return std::string();
    }

    return host.toStdString();
}

//--------------------------------------------------------------------------------------------------
AccountNameCandidates accountNameCandidates(const QString& text)
{
    AccountNameCandidates result;

    auto add = [](QStringList* list, const QString& value)
    {
        if (!value.isEmpty() && !list->contains(value))
            list->append(value);
    };

    const qsizetype backslash = text.indexOf(QLatin1Char('\\'));
    const qsizetype at = text.lastIndexOf(QLatin1Char('@'));

    if (backslash > 0 && backslash < text.size() - 1)
    {
        result.domain = text.left(backslash);
        result.name = text.mid(backslash + 1);
    }
    else if (backslash < 0 && at > 0 && at < text.size() - 1)
    {
        result.name = text.left(at);
        result.domain = text.mid(at + 1);
    }
    else
    {
        result.name = text;
    }

    add(&result.spellings, text);
    add(&result.spellings, text.toLower());

    if (!result.domain.isEmpty())
    {
        const QString slash_form = result.domain + QLatin1Char('\\') + result.name;
        const QString at_form = result.name + QLatin1Char('@') + result.domain;
        add(&result.spellings, slash_form);
        add(&result.spellings, at_form);
        add(&result.spellings, slash_form.toLower());
        add(&result.spellings, at_form.toLower());

        add(&result.short_names, result.name);
        add(&result.short_names, result.name.toLower());
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
namespace {

struct IniSection
{
    QString name;
    QList<QPair<QString, QString>> values;
};

QList<IniSection> parseIni(const QString& text)
{
    QList<IniSection> sections;
    sections.append(IniSection());

    const QStringList lines = text.split(QLatin1Char('\n'));
    for (QString line : lines)
    {
        line = line.trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#')) || line.startsWith(QLatin1Char(';')))
            continue;

        if (line.startsWith(QLatin1Char('[')) && line.endsWith(QLatin1Char(']')))
        {
            IniSection section;
            section.name = line.mid(1, line.size() - 2).trimmed();
            sections.append(section);
            continue;
        }

        const qsizetype eq = line.indexOf(QLatin1Char('='));
        if (eq <= 0)
            continue;

        sections.last().values.append(
            { line.left(eq).trimmed().toLower(), line.mid(eq + 1).trimmed() });
    }

    return sections;
}

QString iniValue(const QList<IniSection>& sections, const QString& section_name, const QString& key)
{
    QString result;
    for (const IniSection& section : sections)
    {
        if (section.name.compare(section_name, Qt::CaseInsensitive) != 0)
            continue;

        // The last value wins, as in sssd and samba (conf.d files come after the main file).
        for (const auto& value : section.values)
        {
            if (value.first == key)
                result = value.second;
        }
    }
    return result;
}

void addAlias(QStringList* aliases, const QString& value)
{
    const QString alias = value.trimmed();
    if (alias.isEmpty())
        return;

    if (!aliases->contains(alias, Qt::CaseInsensitive))
        aliases->append(alias);

    const qsizetype dot = alias.indexOf(QLatin1Char('.'));
    if (dot > 0)
    {
        const QString label = alias.left(dot);
        if (!aliases->contains(label, Qt::CaseInsensitive))
            aliases->append(label);
    }
}

} // namespace

//--------------------------------------------------------------------------------------------------
QStringList sssdDefaultDomainAliases(const QString& conf_text)
{
    const QList<IniSection> sections = parseIni(conf_text);

    QString domain = iniValue(sections, QStringLiteral("sssd"), QStringLiteral("default_domain_suffix"));
    if (domain.isEmpty())
    {
        const QStringList domains =
            iniValue(sections, QStringLiteral("sssd"), QStringLiteral("domains"))
                .split(QLatin1Char(','), Qt::SkipEmptyParts);
        if (!domains.isEmpty())
            domain = domains.first().trimmed();
    }

    QStringList aliases;
    if (domain.isEmpty())
        return aliases;

    addAlias(&aliases, domain);

    const QString section = QStringLiteral("domain/") + domain;
    static const char* const kKeys[] = { "ad_domain", "krb5_realm", "ipa_domain", "dns_discovery_domain" };
    for (const char* key : kKeys)
        addAlias(&aliases, iniValue(sections, section, QString::fromLatin1(key)));

    return aliases;
}

//--------------------------------------------------------------------------------------------------
QStringList sambaDefaultDomainAliases(const QString& conf_text)
{
    const QList<IniSection> sections = parseIni(conf_text);

    QStringList aliases;
    addAlias(&aliases, iniValue(sections, QStringLiteral("global"), QStringLiteral("workgroup")));
    addAlias(&aliases, iniValue(sections, QStringLiteral("global"), QStringLiteral("realm")));
    return aliases;
}

//--------------------------------------------------------------------------------------------------
bool isDefaultDomain(const QString& domain, const QStringList& aliases)
{
    const QString folded = domain.trimmed().toCaseFolded();
    if (folded.isEmpty())
        return false;

    for (const QString& alias : aliases)
    {
        if (alias.trimmed().toCaseFolded() == folded)
            return true;
    }
    return false;
}

//--------------------------------------------------------------------------------------------------
QString domainOfName(const QString& name)
{
    const qsizetype backslash = name.indexOf(QLatin1Char('\\'));
    if (backslash > 0)
        return name.left(backslash);

    const qsizetype at = name.lastIndexOf(QLatin1Char('@'));
    if (at > 0 && at < name.size() - 1)
        return name.mid(at + 1);

    return QString();
}

//--------------------------------------------------------------------------------------------------
bool isLocalIdRange(quint32 id, quint32 min_id)
{
    if (id < min_id)
        return true;
    if (id == 65534)
        return true;
    if (id >= 61184 && id <= 65519)
        return true;
    return false;
}

} // namespace domain_logon_pam

namespace {

//--------------------------------------------------------------------------------------------------
DomainAuthenticator::Status statusFromPam(int rc)
{
    switch (rc)
    {
        case PAM_AUTH_ERR:
        case PAM_USER_UNKNOWN:
        case PAM_CRED_INSUFFICIENT:
            return DomainAuthenticator::Status::INVALID_CREDENTIALS;

        case PAM_MAXTRIES:
            return DomainAuthenticator::Status::ACCOUNT_LOCKED;

        case PAM_ACCT_EXPIRED:
            return DomainAuthenticator::Status::ACCOUNT_EXPIRED;

        case PAM_NEW_AUTHTOK_REQD:
        case PAM_AUTHTOK_EXPIRED:
            return DomainAuthenticator::Status::CREDENTIALS_EXPIRED;

        case PAM_PERM_DENIED:
            return DomainAuthenticator::Status::LOGON_DENIED;

        default:
            return DomainAuthenticator::Status::INTERNAL_ERROR;
    }
}

//--------------------------------------------------------------------------------------------------
// The name PAM and NSS expect. sssd accepts "user@domain" and "user"; winbind is usually configured
// for "DOMAIN\user" (winbind separator) or "user" with "winbind use default domain". The client
// sends "DOMAIN\user" split into |domain| and |username|, which is joined back here.
std::string logonName(const QString& domain, const QString& username)
{
    if (domain.isEmpty())
        return username.toStdString();

    return QString(domain + QLatin1Char('\\') + username).toStdString();
}

//--------------------------------------------------------------------------------------------------
std::vector<char> nssBuffer(int name)
{
    long size = sysconf(name);
    if (size <= 0)
        size = 16384;
    return std::vector<char>(static_cast<size_t>(size));
}

//--------------------------------------------------------------------------------------------------
bool findGroupById(gid_t gid, QString* name)
{
    std::vector<char> buffer = nssBuffer(_SC_GETGR_R_SIZE_MAX);
    struct group grp;
    struct group* gr = nullptr;

    int error = getgrgid_r(gid, &grp, buffer.data(), buffer.size(), &gr);
    if (error == ERANGE)
    {
        buffer.resize(buffer.size() * 8);
        error = getgrgid_r(gid, &grp, buffer.data(), buffer.size(), &gr);
    }

    if (error != 0 || !gr)
        return false;

    *name = QString::fromUtf8(gr->gr_name);
    return true;
}

//--------------------------------------------------------------------------------------------------
bool findGroupByName(const QString& name, gid_t* gid, QString* canonical_name)
{
    const std::string name_utf8 = name.toStdString();
    std::vector<char> buffer = nssBuffer(_SC_GETGR_R_SIZE_MAX);
    struct group grp;
    struct group* gr = nullptr;

    int error = getgrnam_r(name_utf8.c_str(), &grp, buffer.data(), buffer.size(), &gr);
    if (error == ERANGE)
    {
        buffer.resize(buffer.size() * 8);
        error = getgrnam_r(name_utf8.c_str(), &grp, buffer.data(), buffer.size(), &gr);
    }

    if (error != 0 || !gr)
        return false;

    *gid = gr->gr_gid;
    *canonical_name = QString::fromUtf8(gr->gr_name);
    return true;
}

//--------------------------------------------------------------------------------------------------
bool findUserByName(const QString& name, uid_t* uid, QString* canonical_name, gid_t* primary_gid)
{
    const std::string name_utf8 = name.toStdString();
    std::vector<char> buffer = nssBuffer(_SC_GETPW_R_SIZE_MAX);
    struct passwd pwd;
    struct passwd* pw = nullptr;

    int error = getpwnam_r(name_utf8.c_str(), &pwd, buffer.data(), buffer.size(), &pw);
    if (error == ERANGE)
    {
        buffer.resize(buffer.size() * 8);
        error = getpwnam_r(name_utf8.c_str(), &pwd, buffer.data(), buffer.size(), &pw);
    }

    if (error != 0 || !pw)
        return false;

    *uid = pw->pw_uid;
    *primary_gid = pw->pw_gid;
    *canonical_name = QString::fromUtf8(pw->pw_name);
    return true;
}

//--------------------------------------------------------------------------------------------------
// True if |name| is in the local files database (/etc/passwd or /etc/group). Such a name is not
// a domain account, whatever domain the administrator wrote in front of it.
bool isLocalFilesName(const char* path, const QString& name, bool missing_is_local = true)
{
    FILE* file = fopen(path, "re");
    if (!file)
        return missing_is_local; // /etc files can not be read: refuse the short-name fallback.

    const std::string name_utf8 = name.toStdString();
    bool found = false;
    char* line = nullptr;
    size_t capacity = 0;

    while (getline(&line, &capacity, file) != -1)
    {
        const char* colon = strchr(line, ':');
        if (!colon)
            continue;

        const size_t length = static_cast<size_t>(colon - line);
        if (length == name_utf8.size() && strncasecmp(line, name_utf8.c_str(), length) == 0)
        {
            found = true;
            break;
        }
    }

    free(line);
    fclose(file);
    return found;
}

//--------------------------------------------------------------------------------------------------
QString readTextFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return QString();
    return QString::fromUtf8(file.read(1024 * 1024));
}

//--------------------------------------------------------------------------------------------------
// Names of the domain this computer is joined to (sssd or winbind). Empty if neither is
// configured or the configuration can not be read (the short-name fallback is then refused).
QStringList defaultDomainAliases()
{
    QString sssd = readTextFile(QStringLiteral("/etc/sssd/sssd.conf"));
    if (!sssd.isEmpty())
    {
        const QDir conf_d(QStringLiteral("/etc/sssd/conf.d"));
        const QStringList files = conf_d.entryList({ QStringLiteral("*.conf") }, QDir::Files, QDir::Name);
        for (const QString& file : files)
            sssd += QLatin1Char('\n') + readTextFile(conf_d.filePath(file));
    }

    QStringList aliases = domain_logon_pam::sssdDefaultDomainAliases(sssd);
    const QStringList samba =
        domain_logon_pam::sambaDefaultDomainAliases(readTextFile(QStringLiteral("/etc/samba/smb.conf")));
    for (const QString& alias : samba)
    {
        if (!aliases.contains(alias, Qt::CaseInsensitive))
            aliases.append(alias);
    }
    return aliases;
}

//--------------------------------------------------------------------------------------------------
// UID_MIN / GID_MIN of /etc/login.defs (1000 if not set).
quint32 loginDefsMin(const char* key)
{
    const QStringList lines = readTextFile(QStringLiteral("/etc/login.defs")).split(QLatin1Char('\n'));
    for (const QString& line : lines)
    {
        const QStringList parts = line.simplified().split(QLatin1Char(' '));
        if (parts.size() >= 2 && parts[0] == QLatin1String(key))
        {
            bool ok = false;
            const quint32 value = parts[1].toUInt(&ok);
            if (ok)
                return value;
        }
    }
    return 1000;
}

//--------------------------------------------------------------------------------------------------
// The short-name fallback must never take a local account for a domain one. glibc does not tell
// which NSS module answered, so the local sources that can be checked are checked (files,
// extrausers), and system / dynamic id ranges are refused (they are never domain ids).
bool isLocalAccount(bool is_group, const QString& name, quint32 id)
{
    if (is_group)
    {
        if (isLocalFilesName("/etc/group", name) || isLocalFilesName("/var/lib/extrausers/group", name, false))
            return true;
        return domain_logon_pam::isLocalIdRange(id, loginDefsMin("GID_MIN"));
    }

    if (isLocalFilesName("/etc/passwd", name) || isLocalFilesName("/var/lib/extrausers/passwd", name, false))
        return true;
    return domain_logon_pam::isLocalIdRange(id, loginDefsMin("UID_MIN"));
}

//--------------------------------------------------------------------------------------------------
bool findUserById(uid_t uid, QString* name)
{
    std::vector<char> buffer = nssBuffer(_SC_GETPW_R_SIZE_MAX);
    struct passwd pwd;
    struct passwd* pw = nullptr;

    int error = getpwuid_r(uid, &pwd, buffer.data(), buffer.size(), &pw);
    if (error == ERANGE)
    {
        buffer.resize(buffer.size() * 8);
        error = getpwuid_r(uid, &pwd, buffer.data(), buffer.size(), &pw);
    }

    if (error != 0 || !pw)
        return false;

    *name = QString::fromUtf8(pw->pw_name);
    return true;
}

} // namespace

//--------------------------------------------------------------------------------------------------
DomainLogonResult domainLogon(const QString& domain,
                              const QString& username,
                              const SecureString& password,
                              const QString& peer_address,
                              const QString& pam_service)
{
    DomainLogonResult result;

    QString service_error;
    if (!isUsablePamService(pam_service, &service_error))
    {
        // A missing file would make PAM fall back to the "other" service, which may be permissive.
        LOG(ERROR) << "PAM service is not usable:" << service_error;
        result.status = DomainAuthenticator::Status::UNSUPPORTED;
        return result;
    }

    const std::string user_name = logonName(domain, username);
    const std::string service = pam_service.toStdString();

    domain_logon_pam::PamConversationData data;
    {
        SecureByteArray password_utf8 = password.toUtf8();
        data.password.assign(password_utf8.constData(), static_cast<size_t>(password_utf8.size()));
    }

    struct pam_conv conv;
    conv.conv = domain_logon_pam::pamConversation;
    conv.appdata_ptr = &data;

    pam_handle_t* pamh = nullptr;
    int rc = pam_start(service.c_str(), user_name.c_str(), &conv, &pamh);
    if (rc != PAM_SUCCESS)
    {
        LOG(ERROR) << "pam_start failed for service" << pam_service << ":" << pam_strerror(pamh, rc);
        memZero(&data.password);
        result.status = DomainAuthenticator::Status::INTERNAL_ERROR;
        return result;
    }

    // Who asks, for the modules of the stack: pam_faillock and pam_tally count failures per host,
    // pam_access can restrict by address, and the audit log records them. PAM_TTY is required by
    // some modules (pam_access, pam_time); "aspia" is not a real terminal.
    const std::string rhost = domain_logon_pam::remoteHost(peer_address);
    if (!rhost.empty())
        rc = pam_set_item(pamh, PAM_RHOST, rhost.c_str());
    // PAM_RUSER is not set: it names the requesting user on the remote side (rlogin semantics),
    // which is unknown here. Setting it to the target name would let modules like pam_rhosts or a
    // custom policy treat the claimed name as an authenticated remote user.
    if (rc == PAM_SUCCESS)
        rc = pam_set_item(pamh, PAM_TTY, "aspia");
    if (rc != PAM_SUCCESS)
    {
        LOG(ERROR) << "pam_set_item failed:" << pam_strerror(pamh, rc);
        pam_end(pamh, rc);
        memZero(&data.password);
        result.status = DomainAuthenticator::Status::INTERNAL_ERROR;
        return result;
    }

    // PAM_DISALLOW_NULL_AUTHTOK: an account without a password must never pass.
    rc = pam_authenticate(pamh, PAM_SILENT | PAM_DISALLOW_NULL_AUTHTOK);
    if (rc == PAM_SUCCESS)
        rc = pam_acct_mgmt(pamh, PAM_SILENT | PAM_DISALLOW_NULL_AUTHTOK);

    // The name after authentication may be canonicalized by a module (e.g. sssd).
    std::string canonical_name = user_name;
    if (rc == PAM_SUCCESS)
    {
        const void* item = nullptr;
        if (pam_get_item(pamh, PAM_USER, &item) == PAM_SUCCESS && item)
            canonical_name = static_cast<const char*>(item);
    }
    else
    {
        LOG(INFO) << "PAM authentication failed for" << QString::fromStdString(user_name)
                  << "(service" << pam_service << "):" << pam_strerror(pamh, rc);
    }

    pam_end(pamh, rc);
    memZero(&data.password);

    if (rc != PAM_SUCCESS)
    {
        result.status = statusFromPam(rc);
        return result;
    }

    // Groups of the account through NSS (sssd/winbind). getpwnam_r/getgrgid_r are thread-safe.
    long buffer_size = sysconf(_SC_GETPW_R_SIZE_MAX);
    if (buffer_size <= 0)
        buffer_size = 16384;

    std::vector<char> pw_buffer(static_cast<size_t>(buffer_size));
    struct passwd pwd;
    struct passwd* pw = nullptr;

    int pw_error = getpwnam_r(canonical_name.c_str(), &pwd, pw_buffer.data(), pw_buffer.size(), &pw);
    if (pw_error == ERANGE)
    {
        pw_buffer.resize(pw_buffer.size() * 8);
        pw_error = getpwnam_r(canonical_name.c_str(), &pwd, pw_buffer.data(), pw_buffer.size(), &pw);
    }

    if (pw_error != 0 || !pw)
    {
        LOG(ERROR) << "getpwnam_r failed for" << QString::fromStdString(canonical_name)
                   << "error:" << pw_error;
        result.status = DomainAuthenticator::Status::INTERNAL_ERROR;
        return result;
    }

    result.account = QString::fromUtf8(pw->pw_name);

    // The account itself, for rules that name a single user ("uid:<number>").
    DomainLogonGroup self;
    self.id = QStringLiteral("uid:") + QString::number(pw->pw_uid);
    self.is_account = true;
    result.groups.append(self);

    int group_count = 64;
    std::vector<gid_t> gids(static_cast<size_t>(group_count));

    if (getgrouplist(pw->pw_name, pw->pw_gid, gids.data(), &group_count) == -1)
    {
        if (group_count <= 0 || group_count > kMaxGroups)
        {
            LOG(ERROR) << "Unexpected number of groups:" << group_count;
            result.status = DomainAuthenticator::Status::INTERNAL_ERROR;
            return result;
        }

        gids.resize(static_cast<size_t>(group_count));
        if (getgrouplist(pw->pw_name, pw->pw_gid, gids.data(), &group_count) == -1)
        {
            LOG(ERROR) << "getgrouplist failed";
            result.status = DomainAuthenticator::Status::INTERNAL_ERROR;
            return result;
        }
    }

    // Rules are matched by GID, so the names of the groups are not needed.
    for (int i = 0; i < group_count; ++i)
    {
        DomainLogonGroup item;
        item.id = QString::number(gids[static_cast<size_t>(i)]);
        result.groups.append(item);
    }

    result.status = DomainAuthenticator::Status::SUCCESS;
    return result;
}

//--------------------------------------------------------------------------------------------------
bool resolveLegacyDomainRule(const QString& text, DomainAccountInfo* info, QString* error)
{
    if (!info)
        return false;

    const QString value = text.trimmed();
    *info = DomainAccountInfo();

    // "uid:N" and GIDs as before.
    bool numeric = false;
    value.toUInt(&numeric, 10);
    if (value.startsWith(QLatin1String("uid:"), Qt::CaseInsensitive) ||
        (numeric && !value.startsWith(QLatin1Char('+'))))
    {
        return resolveDomainAccount(value, info, error);
    }

    if (value.isEmpty())
    {
        if (error)
            *error = QStringLiteral("The group is empty");
        return false;
    }

    // A group name exactly as typed, as older versions did. Never a user and no other spellings:
    // an upgrade must not silently grant access that the rule did not grant before.
    gid_t gid = 0;
    QString canonical_name;
    if (!findGroupByName(value, &gid, &canonical_name))
    {
        uid_t uid = 0;
        gid_t primary_gid = 0;
        QString user_name;
        if (findUserByName(value, &uid, &user_name, &primary_gid))
        {
            LOG(WARNING) << "Legacy domain rule" << value << "names a user (uid" << uid
                         << "), not a group; it is ignored as before. Add it again on the Domain"
                         << "tab (or as \"uid:" << uid << "\") to allow this user";
        }

        if (error)
            *error = QStringLiteral("Group \"%1\" was not found").arg(value);
        return false;
    }

    uid_t uid = 0;
    gid_t primary_gid = 0;
    QString user_name;
    if (findUserByName(value, &uid, &user_name, &primary_gid) && primary_gid != gid)
    {
        LOG(WARNING) << "Legacy domain rule" << value << "is both a group (GID" << gid
                     << ") and a user (uid" << uid << "); it is used as the group, as before";
    }

    info->id = QString::number(gid);
    info->name = canonical_name;
    return true;
}

//--------------------------------------------------------------------------------------------------
bool isUsablePamService(const QString& name, QString* error)
{
    auto fail = [error](const QString& message)
    {
        if (error)
            *error = message;
        return false;
    };

    if (!isValidPamServiceName(name))
        return fail(QStringLiteral("Invalid PAM service name \"%1\"").arg(name));

    // Linux-PAM reads /etc/pam.d first, then the vendor directories.
    static const char* const kDirs[] = { "/etc/pam.d/", "/usr/lib/pam.d/", "/usr/share/pam.d/" };
    for (const char* dir : kDirs)
    {
        const std::string path = std::string(dir) + name.toStdString();
        if (access(path.c_str(), R_OK) == 0)
            return true;
    }

    return fail(QStringLiteral("PAM service file \"%1\" does not exist in /etc/pam.d").arg(name));
}

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

    bool ok = false;

    // A single user: "uid:1234".
    if (value.startsWith(QLatin1String("uid:"), Qt::CaseInsensitive))
    {
        const uint uid = value.mid(4).toUInt(&ok, 10);
        if (!ok || value.mid(4).startsWith(QLatin1Char('+')))
            return fail(QStringLiteral("Invalid user id"));

        info->id = QStringLiteral("uid:") + QString::number(uid);
        info->kind = DomainGroupRule::Kind::USER;
        findUserById(static_cast<uid_t>(uid), &info->name);
        return true;
    }

    // A GID. It is accepted even if NSS does not know it right now (e.g. sssd is offline).
    const uint gid = value.toUInt(&ok, 10);
    if (ok && !value.startsWith(QLatin1Char('+')))
    {
        info->id = QString::number(gid);
        findGroupById(static_cast<gid_t>(gid), &info->name);
        return true;
    }

    // A name: a group or a single user, in any of the forms "DOMAIN\\name", "name@domain",
    // "name@domain.fqdn" or "name", any case. Rules are stored and matched by the resolved
    // identifier only (GID or "uid:N"), never by name.
    enum class Want { ANY, GROUP, USER };
    Want want = Want::ANY;
    QString name_text = value;
    if (name_text.startsWith(QLatin1String("group:"), Qt::CaseInsensitive))
    {
        want = Want::GROUP;
        name_text = name_text.mid(6).trimmed();
    }
    else if (name_text.startsWith(QLatin1String("user:"), Qt::CaseInsensitive))
    {
        want = Want::USER;
        name_text = name_text.mid(5).trimmed();
    }

    if (name_text.isEmpty())
        return fail(QStringLiteral("The account name is empty"));

    const domain_logon_pam::AccountNameCandidates candidates =
        domain_logon_pam::accountNameCandidates(name_text);

    bool group_found = false;
    gid_t found_gid = 0;
    QString group_name;

    bool user_found = false;
    uid_t found_uid = 0;
    gid_t user_primary_gid = 0;
    QString user_name;

    QStringList aliases;

    // A result of the short-name fallback is accepted only if it is not a local account and, when
    // NSS returns a qualified name, that name is in the typed (= joined) domain.
    auto isFallbackResult = [&](bool is_group, const QString& name, quint32 id)
    {
        if (isLocalAccount(is_group, name, id))
            return false;

        const QString domain = domain_logon_pam::domainOfName(name);
        if (!domain.isEmpty() && !domain_logon_pam::isDefaultDomain(domain, aliases) &&
            !domain_logon_pam::isDefaultDomain(domain, { candidates.domain }))
        {
            return false;
        }
        return true;
    };

    auto lookup = [&](const QStringList& spellings, bool short_name)
    {
        for (const QString& spelling : spellings)
        {
            gid_t gid = 0;
            QString name;
            if (!group_found && want != Want::USER && findGroupByName(spelling, &gid, &name) &&
                !(short_name && !isFallbackResult(true, name, gid)))
            {
                group_found = true;
                found_gid = gid;
                group_name = name;
            }

            uid_t uid = 0;
            gid_t primary_gid = 0;
            if (!user_found && want != Want::GROUP &&
                findUserByName(spelling, &uid, &name, &primary_gid) &&
                !(short_name && !isFallbackResult(false, name, uid)))
            {
                user_found = true;
                found_uid = uid;
                user_primary_gid = primary_gid;
                user_name = name;
            }
        }
    };

    lookup(candidates.spellings, false);

    // "DOMAIN\\name" that NSS does not know in a qualified form: the name alone resolves in the
    // default domain of sssd / winbind only, so it is tried only if DOMAIN is that domain (by its
    // DNS or NetBIOS name). Otherwise "PARTNER\\ivanov" could become the "ivanov" of the joined
    // domain. A local account of that name is never taken for it.
    if (!group_found && !user_found && !candidates.short_names.isEmpty())
    {
        aliases = defaultDomainAliases();
        if (!domain_logon_pam::isDefaultDomain(candidates.domain, aliases))
        {
            return fail(QStringLiteral(
                "Group or user \"%1\" was not found, and the domain \"%2\" is not the domain this "
                "computer is joined to (%3), so the name is not looked up without it. Write the "
                "account exactly as the system shows it (commands getent group, getent passwd or "
                "id), or enter the GID of a group or \"uid:N\" for a single user.")
                .arg(name_text, candidates.domain,
                     aliases.isEmpty() ? QStringLiteral("none found in sssd.conf / smb.conf")
                                       : aliases.join(QStringLiteral(", "))));
        }

        lookup(candidates.short_names, true);
    }

    // A user private group (sssd auto_private_groups, local UPG): the group of the same name is
    // the primary group of that user, so the logon of that user matches it (getgrouplist() returns
    // the primary group too). Kept as the group, as older versions resolved it.
    if (group_found && user_found && found_gid == user_primary_gid &&
        group_name.compare(user_name, Qt::CaseInsensitive) == 0)
    {
        user_found = false;
    }

    if (group_found && user_found)
    {
        return fail(QStringLiteral(
            "\"%1\" is both a group (GID %2) and a user (uid %3). Enter the GID for the group "
            "or \"uid:%3\" for the user.").arg(name_text).arg(found_gid).arg(found_uid));
    }

    if (group_found)
    {
        info->id = QString::number(found_gid);
        info->name = group_name;
        return true;
    }

    if (user_found)
    {
        info->id = QStringLiteral("uid:") + QString::number(found_uid);
        info->name = user_name;
        info->kind = DomainGroupRule::Kind::USER;
        return true;
    }

    // No shell commands with the name filled in: a name with a quote in it would turn a copied
    // suggestion into a different command.
    return fail(QStringLiteral(
        "Group or user \"%1\" was not found. Check how the system sees the account with the "
        "commands getent group, getent passwd or id (try also the forms name@domain and the name "
        "alone). You can also enter the GID of a group or \"uid:N\" for a single user.")
        .arg(name_text));
}
