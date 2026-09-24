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

#include "host/host_storage.h"

#include <ctime>

namespace {

const qint64 kCountPeriod = 7 * 24 * 60 * 60; // Seconds.
const qsizetype kMaxTimepoints = 1000;

//--------------------------------------------------------------------------------------------------
QList<qint64> readTimepoints(const QSettings& settings, const QString& key)
{
    QList<qint64> timepoints;
    for (const QString& timepoint : settings.value(key).toStringList())
        timepoints.append(timepoint.toLongLong());
    return timepoints;
}

//--------------------------------------------------------------------------------------------------
void writeTimepoints(QSettings& settings, const QString& key, const QList<qint64>& timepoints)
{
    QStringList list;
    for (qint64 timepoint : timepoints)
        list.append(QString::number(timepoint));
    settings.setValue(key, list);
}

//--------------------------------------------------------------------------------------------------
void appendTimepoint(QList<qint64>* timepoints, qint64 timepoint)
{
    while (timepoints->size() >= kMaxTimepoints)
        timepoints->removeFirst();

    timepoints->append(timepoint);
}

//--------------------------------------------------------------------------------------------------
void appendLogin(QList<qint64>* logins, qint64 current_time, qint64 start_time)
{
    logins->removeIf([current_time, start_time](qint64 login)
    {
        return current_time - login >= kCountPeriod && login < start_time;
    });
    appendTimepoint(logins, current_time);
}

//--------------------------------------------------------------------------------------------------
int countAfter(const QList<qint64>& timepoints, qint64 boundary)
{
    int count = 0;
    for (qint64 timepoint : timepoints)
    {
        if (timepoint > boundary)
            ++count;
    }
    return count;
}

} // namespace

//--------------------------------------------------------------------------------------------------
HostStorage::HostStorage()
    // The desktop host runs as a system service and keeps these values machine-wide (SystemScope). On
    // Android the app is sandboxed and cannot write to the system scope, so the values would be lost on
    // every start; the app-private user scope persists them instead.
#if defined(Q_OS_ANDROID)
    : impl_(QSettings::IniFormat, QSettings::UserScope, "aspia", "host_storage")
#else
    : impl_(QSettings::IniFormat, QSettings::SystemScope, "aspia", "host_storage")
#endif
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
HostStorage::~HostStorage() = default;

//--------------------------------------------------------------------------------------------------
HostId HostStorage::lastHostId() const
{
    return impl_.value("host_id").toULongLong();
}

//--------------------------------------------------------------------------------------------------
void HostStorage::setLastHostId(HostId host_id)
{
    impl_.setValue("host_id", host_id);
    impl_.sync();
}

//--------------------------------------------------------------------------------------------------
qint64 HostStorage::lastUpdateCheck() const
{
    return impl_.value("last_update_check").toLongLong();
}

//--------------------------------------------------------------------------------------------------
void HostStorage::setLastUpdateCheck(qint64 timepoint)
{
    impl_.setValue("last_update_check", timepoint);
}

//--------------------------------------------------------------------------------------------------
qint64 HostStorage::updateRetryTime() const
{
    return impl_.value("update_retry_time").toLongLong();
}

//--------------------------------------------------------------------------------------------------
void HostStorage::setUpdateRetryTime(qint64 timepoint)
{
    impl_.setValue("update_retry_time", timepoint);
}

//--------------------------------------------------------------------------------------------------
QString HostStorage::updateCheckResult() const
{
    return impl_.value("update_check_result").toString();
}

//--------------------------------------------------------------------------------------------------
void HostStorage::setUpdateCheckResult(const QString& result)
{
    impl_.setValue("update_check_result", result);
}

//--------------------------------------------------------------------------------------------------
QString HostStorage::updateInstallVersion() const
{
    return impl_.value("update_install_version").toString();
}

//--------------------------------------------------------------------------------------------------
void HostStorage::setUpdateInstallVersion(const QString& version)
{
    impl_.setValue("update_install_version", version);
}

//--------------------------------------------------------------------------------------------------
qint64 HostStorage::serviceStartTime() const
{
    const QList<qint64> starts = serviceStarts();
    return starts.isEmpty() ? 0 : starts.last();
}

//--------------------------------------------------------------------------------------------------
int HostStorage::serviceStartCount() const
{
    return countAfter(serviceStarts(), std::time(nullptr) - kCountPeriod);
}

//--------------------------------------------------------------------------------------------------
void HostStorage::registerServiceStart()
{
    const qint64 current_time = std::time(nullptr);

    QList<qint64> starts = serviceStarts();
    starts.removeIf([current_time](qint64 start)
    {
        return current_time - start >= kCountPeriod;
    });
    appendTimepoint(&starts, current_time);

    setServiceStarts(starts);
}

//--------------------------------------------------------------------------------------------------
qint64 HostStorage::lastClientConnectTime() const
{
    return impl_.value("last_client_connect_time").toLongLong();
}

//--------------------------------------------------------------------------------------------------
void HostStorage::setLastClientConnectTime(qint64 timepoint)
{
    impl_.setValue("last_client_connect_time", timepoint);
}

//--------------------------------------------------------------------------------------------------
int HostStorage::successfulLoginCount() const
{
    return countAfter(successfulLogins(), std::time(nullptr) - kCountPeriod);
}

//--------------------------------------------------------------------------------------------------
int HostStorage::successfulLoginCountSinceStart() const
{
    return countAfter(successfulLogins(), serviceStartTime() - 1);
}

//--------------------------------------------------------------------------------------------------
void HostStorage::registerSuccessfulLogin()
{
    QList<qint64> successful_logins = successfulLogins();
    appendLogin(&successful_logins, std::time(nullptr), serviceStartTime());
    setSuccessfulLogins(successful_logins);
}

//--------------------------------------------------------------------------------------------------
int HostStorage::failedLoginCount() const
{
    return countAfter(failedLogins(), std::time(nullptr) - kCountPeriod);
}

//--------------------------------------------------------------------------------------------------
int HostStorage::failedLoginCountSinceStart() const
{
    return countAfter(failedLogins(), serviceStartTime() - 1);
}

//--------------------------------------------------------------------------------------------------
void HostStorage::registerFailedLogin()
{
    QList<qint64> failed_logins = failedLogins();
    appendLogin(&failed_logins, std::time(nullptr), serviceStartTime());
    setFailedLogins(failed_logins);
}

//--------------------------------------------------------------------------------------------------
QList<qint64> HostStorage::serviceStarts() const
{
    return readTimepoints(impl_, "service_starts");
}

//--------------------------------------------------------------------------------------------------
void HostStorage::setServiceStarts(const QList<qint64>& starts)
{
    writeTimepoints(impl_, "service_starts", starts);
}

//--------------------------------------------------------------------------------------------------
QList<qint64> HostStorage::successfulLogins() const
{
    return readTimepoints(impl_, "successful_logins");
}

//--------------------------------------------------------------------------------------------------
void HostStorage::setSuccessfulLogins(const QList<qint64>& successful_logins)
{
    writeTimepoints(impl_, "successful_logins", successful_logins);
}

//--------------------------------------------------------------------------------------------------
QList<qint64> HostStorage::failedLogins() const
{
    return readTimepoints(impl_, "failed_logins");
}

//--------------------------------------------------------------------------------------------------
void HostStorage::setFailedLogins(const QList<qint64>& failed_logins)
{
    writeTimepoints(impl_, "failed_logins", failed_logins);
}

//--------------------------------------------------------------------------------------------------
bool HostStorage::isBootToSafeMode() const
{
    return impl_.value("boot_to_safe_mode").toBool();
}

//--------------------------------------------------------------------------------------------------
void HostStorage::setBootToSafeMode(bool enable)
{
    impl_.setValue("boot_to_safe_mode", enable);
}
