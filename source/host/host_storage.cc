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

const qint64 kServiceStartPeriod = 7 * 24 * 60 * 60; // Seconds.
const qsizetype kMaxServiceStarts = 1000;

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
    const qint64 current_time = std::time(nullptr);

    int count = 0;
    for (qint64 start : serviceStarts())
    {
        if (current_time - start < kServiceStartPeriod)
            ++count;
    }

    return count;
}

//--------------------------------------------------------------------------------------------------
void HostStorage::registerServiceStart()
{
    const qint64 current_time = std::time(nullptr);

    QList<qint64> starts = serviceStarts();
    starts.removeIf([current_time](qint64 start)
    {
        return current_time - start >= kServiceStartPeriod;
    });

    // At the limit the oldest start gives way, so that the last start stays known.
    while (starts.size() >= kMaxServiceStarts)
        starts.removeFirst();

    starts.append(current_time);

    setServiceStarts(starts);
}

//--------------------------------------------------------------------------------------------------
QList<qint64> HostStorage::serviceStarts() const
{
    QList<qint64> starts;
    for (const QString& start : impl_.value("service_starts").toStringList())
        starts.append(start.toLongLong());
    return starts;
}

//--------------------------------------------------------------------------------------------------
void HostStorage::setServiceStarts(const QList<qint64>& starts)
{
    QStringList list;
    for (qint64 start : starts)
        list.append(QString::number(start));
    impl_.setValue("service_starts", list);
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
