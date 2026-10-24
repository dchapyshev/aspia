# Operating system / domain account logon: security and deployment

This document covers the `IDENTIFY_PASSWORD` mode (logging on to a host with a Windows/AD account or a Linux account via PAM/sssd/winbind) in builds that support this mode.

## How it is protected

- **Host key and TOFU.** The host has a persistent X25519 key ("identity key"). On the first connection the client shows its SHA-256 fingerprint and pins the key to the address. Credentials are sent only over a channel whose key is derived, among other things, from a DH with this key, so a MITM without the host private key cannot read them. The mode label `aspia/identify-password/v2` is added to the transcript. SRP and anonymous logon are unchanged. The identity key is used only for this mode.
- **Key storage.** On Windows the key is stored in the host database encrypted with DPAPI (machine scope), so a copy of the database is useless on another computer. On Linux `host.db3` has mode 0600. A corrupted or unreadable key is never silently regenerated: OS account logon is disabled until the administrator presses **Regenerate** on the Domain tab. After that, clients will see a key change warning.
- **Constant failure time.** For any failure (wrong password, no matching group, block, malformed input, overload) the host answers 4 s after the credentials were received, or at the next multiple of 4 s. A successful logon is not delayed.
- **Brute-force limiting:**
  - Check and reservation are atomic: at most 1 check per account and at most 2 per source (IPv4 address or IPv6 /64) run at the same time.
  - Per account: 3 free failures, then a block for 1, 2, 4 … 30 min. Per source: 5 free failures, then 30 s, 1 … 30 min. Counters are forgotten after 1 h without failures.
  - Account names are normalized (NFKC, case, `DOMAIN\`, `@suffix`), so different spellings of the same account do not give extra attempts.
  - **Trusted sources:** an address (or /64) that had a successful logon to an account in the last 30 days is not affected by that account's block. An outsider cannot lock the owner out. The source's own limits still apply to it.
  - Tables are bounded (10000 entries). Active blocks and in-flight checks are never evicted; if a table is full of active entries, new keys are refused.
  - While an account or a source is blocked, the OS and the domain are not queried, and the AD lockout counter does not grow.
  - The state is kept in the service memory and is reset when the service restarts. A restart requires administrator rights; in that case AD thresholds and `pam_faillock` provide the protection.
- **Windows:**
  - Credentials are checked with `LogonUserW(LOGON32_LOGON_NETWORK)`.
  - Only "user" type accounts are accepted, refusing on any doubt (fail-closed). Computer accounts, gMSA, accounts whose primary group or membership is Domain Computers / Domain Controllers, and accounts whose type cannot be determined are refused.
  - Only accounts of this computer or of the domain it is joined to are accepted. Users of trusted domains and forests are refused.
  - Rules are matched by SID only.
- **Linux:**
  - PAM (`pam_authenticate` + `pam_acct_mgmt`) is used with `PAM_RHOST` and `PAM_TTY=aspia`. `PAM_RUSER` is not set.
  - The service name is validated (`[A-Za-z0-9._-]{1,64}`, the file must exist in `/etc/pam.d`); otherwise PAM would fall back to the `other` service.
  - The password is given to the first prompt only; OTP and password change prompts fail.
  - Rules are matched by GID / `uid:N`.
  - **Group or user input** on the Domain tab: `DOMAIN\name`, `name@domain`, `name` in any case; `group:` / `user:` prefixes; GID; `uid:N`. If a name is both a group and a user, a prefix, GID or `uid:N` is required.
  - **A short name instead of `DOMAIN\name`** is looked up only if NSS does not know the full form and `DOMAIN` matches the domain the computer is joined to. The domain is detected from `default_domain_suffix` or the first entry of `domains` in sssd.conf (with `ad_domain`, `krb5_realm`, `ipa_domain`, `dns_discovery_domain` and their first label as the NetBIOS name), or from `workgroup` / `realm` in smb.conf. Otherwise an error is returned, so `PARTNER\ivanov` never turns into `ivanov` of the own domain. If sssd.conf is not readable (GUI not running as root), this shortening is not done.
  - A local account is never taken for a domain one: names from `/etc/passwd`, `/etc/group`, `/var/lib/extrausers` are refused, as well as ids in the system range (below `UID_MIN`/`GID_MIN`), 65534 and systemd dynamic ids (61184–65519). **Limitation:** glibc does not tell which NSS module answered, so other local sources (NIS, local LDAP, `altfiles`) are not recognized. On such systems enter domain accounts in full form or by GID / `uid:N`.
  - **Rules from older versions without an identifier** are resolved with the old semantics: `uid:N`, GID or the group name exactly as entered, without a user lookup. An upgrade does not change who gets access. If such a name now means a user, a warning is logged; to allow it, add the rule again.
- **Broad groups** (Everyone, Authenticated Users, Domain Users/Guests/Computers, This Organization, Local account, BUILTIN\Users, etc.) require a confirmation in the settings and are flagged in the log.
- **Security log:** directory 0700, files 0600 (Linux); on Windows the service directory DACL applies. Regular logs (`/var/log/aspia/host` and client logs) are also created with mode 0600 in a 0700 directory.
- **Client online check.** For hosts with OS accounts or without saved credentials, the client starts an IDENTIFY_PASSWORD handshake without a pinned key and stops at the host key. No credentials are sent, the limiter is not touched, and it does not count as a logon attempt.
  - The host answers such a check even when the source is blocked. The answer reveals only availability and the public key, which is shown anyway during TOFU.
  - ACCESS_DENIED / SESSION_DENIED / VERSION_ERROR answers also mean "online" (the host answered). Rejected saved credentials show up only on connect. This is by design: the check must not verify passwords.
  - In the security log such connections are written as WARNING `[connection closed before credentials]` at most once per 10 minutes per address, with the number of skipped entries. Failed logons are logged in full.
- **Client passwords.** Saved passwords (including OS / domain accounts) are encrypted with the master password key; `client.db3` has mode 0600 in a 0700 directory (Linux).
  - **Automatic unlock.** On Windows the key is protected with user DPAPI. On Linux it is encrypted with a random key from `~/.config/aspia/os_crypt.key` (0600, directory 0700). The key of older versions, derived from `/etc/machine-id`, is re-encrypted automatically on first open.
  - The file key protects against other local users, but not against processes running under the same account or root. On shared computers do not enable automatic unlock if you save domain passwords.

## Deployment checklist

1. Enable the mode only on the hosts that need it. Firewall the host port (8050) and allow it only from corporate networks or VPN.
2. Create dedicated groups for access (for example, `aspia-users`). Do not use Users, Domain Users, Authenticated Users, Everyone.
3. AD: lockout threshold of at least 10, reset window 15–30 min. Monitor events 4625 and 4740.
4. Linux: after an upgrade, replace `/etc/pam.d/aspia-domain` with the new template from `/usr/share/aspia/` (the installer does not overwrite a modified file). Enable `pam_faillock`: on Debian/Ubuntu `authfail` in `common-auth`, on RHEL `authselect enable-feature with-faillock`. Check that `id user@domain` shows the domain groups.
5. Give users the host key fingerprint over a separate channel. Investigate every key change warning.
6. Do not keep `host.db3` in unencrypted backups. On Linux it contains the private identity key.
7. Monitor the host security log (blocks, "another logon … in progress"), sssd and auth logs.
8. Keep time synchronized on hosts and domain controllers.
