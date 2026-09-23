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

#ifndef HOST_HOST_STORAGE_H
#define HOST_HOST_STORAGE_H

#include <QSettings>

#include "base/peer/host_id.h"

class HostStorage
{
public:
    HostStorage();
    ~HostStorage();

    HostId lastHostId() const;
    void setLastHostId(HostId host_id);

    qint64 lastUpdateCheck() const;
    void setLastUpdateCheck(qint64 timepoint);

    qint64 updateRetryTime() const;
    void setUpdateRetryTime(qint64 timepoint);

    QString updateCheckResult() const;
    void setUpdateCheckResult(const QString& result);

    QString updateInstallVersion() const;
    void setUpdateInstallVersion(const QString& version);

    // The last start of the service and the number of its starts in the last 7 days.
    qint64 serviceStartTime() const;
    int serviceStartCount() const;
    void registerServiceStart();

    bool isBootToSafeMode() const;
    void setBootToSafeMode(bool enable);

private:
    QList<qint64> serviceStarts() const;
    void setServiceStarts(const QList<qint64>& starts);

    QSettings impl_;

    Q_DISABLE_COPY_MOVE(HostStorage)
};

#endif // HOST_HOST_STORAGE_H
