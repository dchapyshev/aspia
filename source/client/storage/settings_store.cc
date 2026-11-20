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

#include "client/storage/settings_store.h"

#include <QStandardPaths>

#include "base/logging.h"
#include "base/sql/sql_query.h"

//--------------------------------------------------------------------------------------------------
SettingsStore::SettingsStore(Database& db)
    : DatabaseStore(db)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
QString SettingsStore::name() const
{
    return readSetting(kSettingName);
}

//--------------------------------------------------------------------------------------------------
bool SettingsStore::setName(const QString& name)
{
    return writeSetting(kSettingName, name);
}

//--------------------------------------------------------------------------------------------------
QString SettingsStore::displayName() const
{
    return readSetting(kSettingDisplayName);
}

//--------------------------------------------------------------------------------------------------
bool SettingsStore::setDisplayName(const QString& name)
{
    return writeSetting(kSettingDisplayName, name);
}

//--------------------------------------------------------------------------------------------------
bool SettingsStore::isBackupOnStartupEnabled() const
{
    return readSetting(kSettingBackupEnabled) == "1";
}

//--------------------------------------------------------------------------------------------------
bool SettingsStore::setBackupOnStartupEnabled(bool enable)
{
    return writeSetting(kSettingBackupEnabled, enable ? "1" : "0");
}

//--------------------------------------------------------------------------------------------------
QString SettingsStore::backupPath() const
{
    QString value = readSetting(kSettingBackupPath);
    if (value.isEmpty())
        value = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/Aspia/Backups";
    return value;
}

//--------------------------------------------------------------------------------------------------
bool SettingsStore::setBackupPath(const QString& path)
{
    return writeSetting(kSettingBackupPath, path);
}

//--------------------------------------------------------------------------------------------------
AutoBackup::Retention SettingsStore::backupRetention() const
{
    bool ok = false;
    const int value = readSetting(kSettingBackupKeep).toInt(&ok);
    if (!ok || value < static_cast<int>(AutoBackup::Retention::ONE_WEEK) ||
        value > static_cast<int>(AutoBackup::Retention::ONE_YEAR))
    {
        return AutoBackup::Retention::ONE_MONTH;
    }

    return static_cast<AutoBackup::Retention>(value);
}

//--------------------------------------------------------------------------------------------------
bool SettingsStore::setBackupRetention(AutoBackup::Retention retention)
{
    return writeSetting(kSettingBackupKeep, QString::number(static_cast<int>(retention)));
}

//--------------------------------------------------------------------------------------------------
Minutes SettingsStore::lockTimeout() const
{
    bool ok = false;
    const int value = readSetting(kSettingLockTimeout).toInt(&ok);
    if (!ok || value < 0)
        return Minutes::zero();

    return Minutes(value);
}

//--------------------------------------------------------------------------------------------------
bool SettingsStore::setLockTimeout(Minutes timeout)
{
    return writeSetting(kSettingLockTimeout, QString::number(timeout.count()));
}

//--------------------------------------------------------------------------------------------------
Database::ReadResult SettingsStore::proxyConfig(ProxyConfig* proxy) const
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return Database::ReadResult::FAILED;
    }

    SqlQuery query(db(), "SELECT value FROM settings WHERE name=?");
    query.addText(std::string_view(kSettingProxy));

    const SqlQuery::StepResult step = query.next();
    if (step == SqlQuery::StepResult::FAILED)
    {
        LOG(ERROR) << "Unable to execute query:" << db().lastError();
        return Database::ReadResult::FAILED;
    }

    const QString value = step == SqlQuery::StepResult::ROW ? query.columnText(0) : QString();

    if (!proxy->setEncryptedData(QByteArray::fromBase64(value.toLatin1())))
    {
        LOG(ERROR) << "Unable to read encrypted data of proxy";
        return Database::ReadResult::INCOMPLETE;
    }

    return Database::ReadResult::OK;
}

//--------------------------------------------------------------------------------------------------
bool SettingsStore::setProxyConfig(const ProxyConfig& proxy)
{
    std::optional<QByteArray> encrypted_data = proxy.encryptedData();
    if (!encrypted_data.has_value())
        return false;

    return writeSetting(kSettingProxy, QString::fromLatin1(encrypted_data->toBase64()));
}
