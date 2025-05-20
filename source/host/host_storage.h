//
// Aspia Project
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

#include "base/macros_magic.h"
#include "base/peer/host_id.h"

#include <QSettings>

namespace host {

class HostStorage
{
public:
    HostStorage();
    ~HostStorage();

    QByteArray key(const QString& session_name) const;
    void setKey(const QString& session_name, const QByteArray& key);

    base::HostId lastHostId(const QString& session_name) const;
    void setLastHostId(const QString& session_name, base::HostId host_id);

    QString channelIdForUI() const;
    void setChannelIdForUI(const QString& channel_id);

    qint64 lastUpdateCheck() const;
    void setLastUpdateCheck(qint64 timepoint);

    bool isBootToSafeMode() const;
    void setBootToSafeMode(bool enable);

private:
    QSettings impl_;

    DISALLOW_COPY_AND_ASSIGN(HostStorage);
};

} // namespace host

#endif // HOST_HOST_STORAGE_H
