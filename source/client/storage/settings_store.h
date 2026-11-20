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

#ifndef CLIENT_STORAGE_SETTINGS_STORE_H
#define CLIENT_STORAGE_SETTINGS_STORE_H

#include "base/time_types.h"
#include "client/storage/auto_backup.h"
#include "client/storage/database_store.h"

class SettingsStore final : public DatabaseStore
{
public:
    explicit SettingsStore(Database& db = Database::instance());

    QString name() const;
    bool setName(const QString& name);

    QString displayName() const;
    bool setDisplayName(const QString& name);

    bool isBackupOnStartupEnabled() const;
    bool setBackupOnStartupEnabled(bool enable);

    QString backupPath() const;
    bool setBackupPath(const QString& path);

    AutoBackup::Retention backupRetention() const;
    bool setBackupRetention(AutoBackup::Retention retention);

    Minutes lockTimeout() const;
    bool setLockTimeout(Minutes timeout);

    // Without a saved value the proxy is not used. A value that did not open leaves |proxy| that
    // way and answers INCOMPLETE.
    Database::ReadResult proxyConfig(ProxyConfig* proxy) const;
    bool setProxyConfig(const ProxyConfig& proxy);
};

#endif // CLIENT_STORAGE_SETTINGS_STORE_H
