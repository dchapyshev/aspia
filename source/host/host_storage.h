//
// SmartCafe Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

namespace host {

class HostStorage
{
public:
    HostStorage();
    ~HostStorage();

    QByteArray hostKey() const;
    void setHostKey(const QByteArray& key);

    base::HostId lastHostId() const;
    void setLastHostId(base::HostId host_id);

    QString channelIdForUI() const;
    void setChannelIdForUI(const QString& channel_id);

    qint64 lastUpdateCheck() const;
    void setLastUpdateCheck(qint64 timepoint);

    bool isBootToSafeMode() const;
    void setBootToSafeMode(bool enable);

private:
    QSettings impl_;

    Q_DISABLE_COPY(HostStorage)
};

} // namespace host

#endif // HOST_HOST_STORAGE_H
