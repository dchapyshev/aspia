//
// Aspia Project
// Copyright (C) 2016-2023 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "host/host_ipc_storage.h"

namespace host {

//--------------------------------------------------------------------------------------------------
HostIpcStorage::HostIpcStorage()
    : impl_(base::JsonSettings::Scope::SYSTEM, "aspia", "host_ipc")
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
HostIpcStorage::~HostIpcStorage() = default;

//--------------------------------------------------------------------------------------------------
std::u16string HostIpcStorage::channelIdForUI() const
{
    return impl_.get<std::u16string>("ui_channel_id");
}

//--------------------------------------------------------------------------------------------------
void HostIpcStorage::setChannelIdForUI(const std::u16string& channel_id)
{
    impl_.set<std::u16string>("ui_channel_id", channel_id);
    impl_.flush();
}

} // namespace host
