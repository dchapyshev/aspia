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

#include "client/storage/database.h"

#include <QStandardPaths>

#include "base/build_config.h"
#include "base/logging.h"
#include "base/sql/sql_query.h"
#include "base/sql/sql_transaction.h"
#include "proto/peer.h"

namespace {

constexpr auto kSettingDisplayName   = "display_name";
constexpr auto kSettingConfirmClose  = "confirm_close";
constexpr auto kSettingCheckUpdates  = "check_updates";
constexpr auto kSettingUpdateChannel = "update_channel";
constexpr auto kSettingBackupEnabled = "backup_on_startup";
constexpr auto kSettingBackupPath    = "backup_path";
constexpr auto kSettingBackupKeep    = "backup_retention";
constexpr auto kSettingLockTimeout   = "lock_timeout";
constexpr auto kSettingProxy         = "proxy";
constexpr auto kSettingMcpEnabled    = "mcp_enabled";
constexpr auto kSettingMcpConfirm    = "mcp_auto_confirm";
constexpr auto kSettingMcpSessions   = "mcp_session_types";
constexpr auto kSettingMcpSignIn     = "mcp_sign_in";
constexpr auto kSettingSalt          = "master_password_salt";
constexpr auto kSettingVerifier      = "master_password_verifier";
constexpr auto kSettingVersion       = "master_password_version";
constexpr auto kSettingBiometricBlob = "biometric_blob";
constexpr auto kSettingAutoUnlock    = "auto_unlock";

} // namespace

//--------------------------------------------------------------------------------------------------
QString Database::displayName() const
{
    return readSetting(kSettingDisplayName);
}

//--------------------------------------------------------------------------------------------------
bool Database::setDisplayName(const QString& name)
{
    return writeSetting(kSettingDisplayName, name);
}

//--------------------------------------------------------------------------------------------------
bool Database::isConfirmCloseEnabled() const
{
    return readSetting(kSettingConfirmClose) == "1";
}

//--------------------------------------------------------------------------------------------------
bool Database::setConfirmCloseEnabled(bool enable)
{
    return writeSetting(kSettingConfirmClose, enable ? "1" : "0");
}

//--------------------------------------------------------------------------------------------------
bool Database::isCheckUpdatesEnabled() const
{
    QString value = readSetting(kSettingCheckUpdates);
    if (value.isEmpty())
        return true;
    return value == "1";
}

//--------------------------------------------------------------------------------------------------
bool Database::setCheckUpdatesEnabled(bool enable)
{
    return writeSetting(kSettingCheckUpdates, enable ? "1" : "0");
}

//--------------------------------------------------------------------------------------------------
QString Database::updateChannel() const
{
    QString value = readSetting(kSettingUpdateChannel);
    if (value.isEmpty())
        value = kStableUpdateChannel;
    return value;
}

//--------------------------------------------------------------------------------------------------
bool Database::setUpdateChannel(const QString& channel)
{
    return writeSetting(kSettingUpdateChannel, channel);
}

//--------------------------------------------------------------------------------------------------
bool Database::isBackupOnStartupEnabled() const
{
    return readSetting(kSettingBackupEnabled) == "1";
}

//--------------------------------------------------------------------------------------------------
bool Database::setBackupOnStartupEnabled(bool enable)
{
    return writeSetting(kSettingBackupEnabled, enable ? "1" : "0");
}

//--------------------------------------------------------------------------------------------------
QString Database::backupPath() const
{
    QString value = readSetting(kSettingBackupPath);
    if (value.isEmpty())
        value = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/Aspia/Backups";
    return value;
}

//--------------------------------------------------------------------------------------------------
bool Database::setBackupPath(const QString& path)
{
    return writeSetting(kSettingBackupPath, path);
}

//--------------------------------------------------------------------------------------------------
AutoBackup::Retention Database::backupRetention() const
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
bool Database::setBackupRetention(AutoBackup::Retention retention)
{
    return writeSetting(kSettingBackupKeep, QString::number(static_cast<int>(retention)));
}

//--------------------------------------------------------------------------------------------------
Minutes Database::lockTimeout() const
{
    bool ok = false;
    const int value = readSetting(kSettingLockTimeout).toInt(&ok);
    if (!ok || value < 0)
        return Minutes::zero();

    return Minutes(value);
}

//--------------------------------------------------------------------------------------------------
bool Database::setLockTimeout(Minutes timeout)
{
    return writeSetting(kSettingLockTimeout, QString::number(timeout.count()));
}

//--------------------------------------------------------------------------------------------------
Database::ReadResult Database::proxyConfig(ProxyConfig* proxy) const
{
    if (!proxy->setEncryptedData(QByteArray::fromBase64(readSetting(kSettingProxy).toLatin1())))
    {
        LOG(ERROR) << "Unable to read encrypted data of proxy";
        return ReadResult::INCOMPLETE;
    }

    return ReadResult::OK;
}

//--------------------------------------------------------------------------------------------------
bool Database::setProxyConfig(const ProxyConfig& proxy)
{
    std::optional<QByteArray> encrypted_data = proxy.encryptedData();
    if (!encrypted_data.has_value())
        return false;

    return writeSetting(kSettingProxy, QString::fromLatin1(encrypted_data->toBase64()));
}

//--------------------------------------------------------------------------------------------------
bool Database::isMcpEnabled() const
{
    return readSetting(kSettingMcpEnabled) == "1";
}

//--------------------------------------------------------------------------------------------------
bool Database::setMcpEnabled(bool enable)
{
    return writeSetting(kSettingMcpEnabled, enable ? "1" : "0");
}

//--------------------------------------------------------------------------------------------------
bool Database::isMcpAutoConfirmEnabled() const
{
    return readSetting(kSettingMcpConfirm) == "1";
}

//--------------------------------------------------------------------------------------------------
bool Database::setMcpAutoConfirmEnabled(bool enable)
{
    return writeSetting(kSettingMcpConfirm, enable ? "1" : "0");
}

//--------------------------------------------------------------------------------------------------
bool Database::isMcpSignInEnabled() const
{
    return readSetting(kSettingMcpSignIn) != "0";
}

//--------------------------------------------------------------------------------------------------
bool Database::setMcpSignInEnabled(bool enable)
{
    return writeSetting(kSettingMcpSignIn, enable ? "1" : "0");
}

//--------------------------------------------------------------------------------------------------
quint32 Database::mcpSessionTypes() const
{
    bool ok = false;
    const quint32 value = readSetting(kSettingMcpSessions).toUInt(&ok);
    if (!ok)
        return proto::peer::SESSION_TYPE_ALL;

    return value;
}

//--------------------------------------------------------------------------------------------------
bool Database::setMcpSessionTypes(quint32 session_types)
{
    return writeSetting(kSettingMcpSessions, QString::number(session_types));
}

//--------------------------------------------------------------------------------------------------
bool Database::isMasterPasswordSet() const
{
    SqlTransaction transaction(db_);
    if (!transaction.begin())
    {
        LOG(ERROR) << "Unable to start transaction:" << db_.lastError();
        return false;
    }

    return !readSetting(kSettingSalt).isEmpty() && !readSetting(kSettingVerifier).isEmpty();
}

//--------------------------------------------------------------------------------------------------
QByteArray Database::masterPasswordSalt() const
{
    return QByteArray::fromBase64(readSetting(kSettingSalt).toLatin1());
}

//--------------------------------------------------------------------------------------------------
QByteArray Database::masterPasswordVerifier() const
{
    return QByteArray::fromBase64(readSetting(kSettingVerifier).toLatin1());
}

//--------------------------------------------------------------------------------------------------
quint32 Database::masterPasswordVersion() const
{
    return readSetting(kSettingVersion).toUInt();
}

//--------------------------------------------------------------------------------------------------
QByteArray Database::autoUnlockBlob() const
{
    return QByteArray::fromBase64(readSetting(kSettingAutoUnlock).toLatin1());
}

//--------------------------------------------------------------------------------------------------
bool Database::setAutoUnlockBlob(const QByteArray& blob)
{
    return writeSetting(kSettingAutoUnlock, QString::fromLatin1(blob.toBase64()));
}

//--------------------------------------------------------------------------------------------------
bool Database::isBiometricUnlockEnabled() const
{
    // The presence of a wrapped key is the single source of truth for the feature being enabled.
    return !biometricBlob().isEmpty();
}

//--------------------------------------------------------------------------------------------------
QByteArray Database::biometricBlob() const
{
    return QByteArray::fromBase64(readSetting(kSettingBiometricBlob).toLatin1());
}

//--------------------------------------------------------------------------------------------------
bool Database::setBiometricBlob(const QByteArray& blob)
{
    return writeSetting(kSettingBiometricBlob, QString::fromLatin1(blob.toBase64()));
}

//--------------------------------------------------------------------------------------------------
bool Database::clearBiometricUnlock()
{
    // Overwrite the wrapped key so the stored blob does not survive disabling the feature
    // (the database runs with secure_delete enabled).
    return writeSetting(kSettingBiometricBlob, QString());
}

//--------------------------------------------------------------------------------------------------
bool Database::setMasterPassword(const QByteArray& salt, const QByteArray& verifier, quint32 version)
{
    return writeSetting(kSettingSalt, QString::fromLatin1(salt.toBase64())) &&
           writeSetting(kSettingVerifier, QString::fromLatin1(verifier.toBase64())) &&
           writeSetting(kSettingVersion, QString::number(version));
}

//--------------------------------------------------------------------------------------------------
QString Database::readSetting(const QString& name, bool* found) const
{
    if (found)
        *found = false;

    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return QString();
    }

    SqlQuery query(db_, "SELECT value FROM settings WHERE name=?");
    query.addText(name);

    if (query.next() != SqlQuery::StepResult::ROW)
        return QString();

    if (found)
        *found = true;

    return query.columnText(0);
}

//--------------------------------------------------------------------------------------------------
bool Database::writeSetting(const QString& name, const QString& value)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    SqlQuery query(db_, "INSERT OR REPLACE INTO settings (name, value) VALUES (?, ?)");
    query.addText(name);
    query.addText(value);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return false;
    }

    return true;
}
