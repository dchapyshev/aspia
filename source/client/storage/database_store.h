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

#ifndef CLIENT_STORAGE_DATABASE_STORE_H
#define CLIENT_STORAGE_DATABASE_STORE_H

#include "client/storage/database.h"

class DatabaseStore
{
public:
    static constexpr char kSettingName[] = "database_name";
    static constexpr char kSettingDisplayName[] = "display_name";
    static constexpr char kSettingBackupEnabled[] = "backup_on_startup";
    static constexpr char kSettingBackupPath[] = "backup_path";
    static constexpr char kSettingBackupKeep[] = "backup_retention";
    static constexpr char kSettingLockTimeout[] = "lock_timeout";
    static constexpr char kSettingProxy[] = "proxy";
    static constexpr char kSettingSalt[] = "master_password_salt";
    static constexpr char kSettingVerifier[] = "master_password_verifier";
    static constexpr char kSettingVersion[] = "master_password_version";
    static constexpr char kSettingAutoUnlock[] = "auto_unlock";
    static constexpr char kSettingBiometricBlob[] = "biometric_blob";
    static constexpr char kSettingRouter[] = "router";

    // The id of the router of the old file a database waits for. The row goes once the router is there.
    static constexpr char kSettingRouterId[] = "migration_router_id";

protected:
    explicit DatabaseStore(Database& db)
        : database_(db)
    {
        // Nothing
    }

    SqlDatabase& db() const { return database_.db_; }
    bool isValid() const { return database_.isValid(); }
    QString readSetting(const QString& name) const;
    bool writeSetting(const QString& name, const QString& value);

    Database& database_;
};

#endif // CLIENT_STORAGE_DATABASE_STORE_H
