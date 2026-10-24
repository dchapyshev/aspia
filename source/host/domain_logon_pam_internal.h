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

#ifndef HOST_DOMAIN_LOGON_PAM_INTERNAL_H
#define HOST_DOMAIN_LOGON_PAM_INTERNAL_H

#include <security/pam_appl.h>

#include <string>

#include <QString>
#include <QStringList>

// Parts of domain_logon_pam.cc exposed for unit tests.
namespace domain_logon_pam {

struct PamConversationData
{
    std::string password;
    bool password_given = false; // The password is given to the first echo-off prompt only.
};

// PAM conversation function; |appdata_ptr| is a PamConversationData.
int pamConversation(int num_msg, const struct pam_message** msg, struct pam_response** resp,
                    void* appdata_ptr);

// Value for PAM_RHOST, or an empty string if |address| is not an IP address.
std::string remoteHost(const QString& address);

// How an account name entered by the administrator is looked up in NSS.
struct AccountNameCandidates
{
    QString domain;      // "asutp.tof" for "asutp.tof\\cimmgr" or "cimmgr@asutp.tof"; empty otherwise.
    QString name;        // The name without a domain.
    QStringList spellings; // Exact spellings to try first: as typed, lower case, the other
                           // qualified form ("DOMAIN\\name" <-> "name@domain"). No duplicates.
    QStringList short_names; // Name without the domain (as typed, lower case); filled only when a
                             // domain was given. NSS resolves it in the default domain only, so
                             // the caller has to check that the result is not a local account.
};

// Splits |text| (already trimmed, without "uid:"/"user:"/"group:" prefix). Pure, no NSS.
AccountNameCandidates accountNameCandidates(const QString& text);

// Names of the default (joined) domain from the text of sssd.conf (with conf.d files appended):
// the domain named by default_domain_suffix, otherwise the first one of "domains"; its section
// name, ad_domain, krb5_realm, ipa_domain, dns_discovery_domain and the first label of each of
// them (the NetBIOS name of an AD domain normally equals it). Pure.
QStringList sssdDefaultDomainAliases(const QString& conf_text);

// The same from smb.conf (winbind): workgroup, realm and the first label of the realm. Pure.
QStringList sambaDefaultDomainAliases(const QString& conf_text);

// True if |domain| names one of |aliases| (case-insensitive, Unicode case folding). Pure.
bool isDefaultDomain(const QString& domain, const QStringList& aliases);

// The domain part of a canonical NSS name ("DOM\\name" or "name@dom"), empty if unqualified. Pure.
QString domainOfName(const QString& name);

// True if |id| belongs to the range of local system or dynamic accounts: below |min_id|
// (UID_MIN/GID_MIN of login.defs), nobody/nogroup (65534) or systemd dynamic users
// (61184..65519). Pure.
bool isLocalIdRange(quint32 id, quint32 min_id);

} // namespace domain_logon_pam

#endif // HOST_DOMAIN_LOGON_PAM_INTERNAL_H
