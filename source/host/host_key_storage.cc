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

#include "host/host_key_storage.h"

#include "base/crypto/generic_hash.h"
#include "base/strings/strcat.h"

namespace host {

namespace {

//--------------------------------------------------------------------------------------------------
std::string sessionKey(std::string_view session_name)
{
    base::ByteArray session_hash =
        base::GenericHash::hash(base::GenericHash::Type::BLAKE2s256, session_name);
    return base::strCat({ "session/", base::toHex(session_hash) });
}

//--------------------------------------------------------------------------------------------------
std::string sessionKeyForHostId(std::string_view session_name)
{
    base::ByteArray session_hash =
        base::GenericHash::hash(base::GenericHash::Type::BLAKE2s256, session_name);
    return base::strCat({ "session_host_id/", base::toHex(session_hash) });
}

} // namespace

//--------------------------------------------------------------------------------------------------
HostKeyStorage::HostKeyStorage()
    : impl_(base::JsonSettings::Scope::SYSTEM, "aspia", "host_key")
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
HostKeyStorage::~HostKeyStorage() = default;

//--------------------------------------------------------------------------------------------------
base::ByteArray HostKeyStorage::key(std::string_view session_name) const
{
    if (session_name.empty())
        return impl_.get<base::ByteArray>("console");

    return impl_.get<base::ByteArray>(sessionKey(session_name));
}

//--------------------------------------------------------------------------------------------------
void HostKeyStorage::setKey(std::string_view session_name, const base::ByteArray& key)
{
    if (session_name.empty())
        impl_.set<base::ByteArray>("console", key);
    else
        impl_.set<base::ByteArray>(sessionKey(session_name), key);

    impl_.flush();
}

//--------------------------------------------------------------------------------------------------
base::HostId HostKeyStorage::lastHostId(std::string_view session_name) const
{
    if (session_name.empty())
        return impl_.get<base::HostId>("console_host_id");

    return impl_.get<base::HostId>(sessionKeyForHostId(session_name));
}

//--------------------------------------------------------------------------------------------------
void HostKeyStorage::setLastHostId(std::string_view session_name, base::HostId host_id)
{
    if (session_name.empty())
        impl_.set<base::HostId>("console_host_id", host_id);
    else
        impl_.set<base::HostId>(sessionKeyForHostId(session_name), host_id);

    impl_.flush();
}

} // namespace host
