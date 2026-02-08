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

#include "host/host_storage.h"

#include "base/xml_settings.h"

namespace host {

//--------------------------------------------------------------------------------------------------
HostStorage::HostStorage()
    : impl_(base::XmlSettings::format(), QSettings::SystemScope, "aspia", "host_storage")
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
HostStorage::~HostStorage() = default;

//--------------------------------------------------------------------------------------------------
QByteArray HostStorage::hostKey() const
{
    return impl_.value("host_key").toByteArray();
}

//--------------------------------------------------------------------------------------------------
void HostStorage::setHostKey(const QByteArray& key)
{
    impl_.setValue("host_key", key);
    impl_.sync();
}

//--------------------------------------------------------------------------------------------------
base::HostId HostStorage::lastHostId() const
{
    return impl_.value("host_id").toULongLong();
}

//--------------------------------------------------------------------------------------------------
void HostStorage::setLastHostId(base::HostId host_id)
{
    impl_.setValue("host_id", host_id);
    impl_.sync();
}

//--------------------------------------------------------------------------------------------------
QString HostStorage::channelIdForUI() const
{
    return impl_.value("ui_channel_id").toString();
}

//--------------------------------------------------------------------------------------------------
void HostStorage::setChannelIdForUI(const QString& channel_id)
{
    impl_.setValue("ui_channel_id", channel_id);
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
bool HostStorage::isBootToSafeMode() const
{
    return impl_.value("boot_to_safe_mode").toBool();
}

//--------------------------------------------------------------------------------------------------
void HostStorage::setBootToSafeMode(bool enable)
{
    impl_.setValue("boot_to_safe_mode", enable);
}

} // namespace host
