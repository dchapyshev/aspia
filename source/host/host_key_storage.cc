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

#include "host/host_key_storage.h"

#include "base/crypto/generic_hash.h"

namespace host {

namespace {

//--------------------------------------------------------------------------------------------------
std::string sessionKey(const QString& session_name)
{
    QByteArray key("session/");
    key.append(base::GenericHash::hash(base::GenericHash::Type::BLAKE2s256, session_name.toUtf8()).toHex());
    return key.toStdString();
}

//--------------------------------------------------------------------------------------------------
std::string sessionKeyForHostId(const QString& session_name)
{
    QByteArray key("session_host_id/");
    key.append(base::GenericHash::hash(base::GenericHash::Type::BLAKE2s256, session_name.toUtf8()).toHex());
    return key.toStdString();
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
QByteArray HostKeyStorage::key(const QString& session_name) const
{
    if (session_name.isEmpty())
        return impl_.get<QByteArray>("console");

    return impl_.get<QByteArray>(sessionKey(session_name));
}

//--------------------------------------------------------------------------------------------------
void HostKeyStorage::setKey(const QString& session_name, const QByteArray& key)
{
    if (session_name.isEmpty())
        impl_.set<QByteArray>("console", key);
    else
        impl_.set<QByteArray>(sessionKey(session_name), key);

    impl_.flush();
}

//--------------------------------------------------------------------------------------------------
base::HostId HostKeyStorage::lastHostId(const QString& session_name) const
{
    if (session_name.isEmpty())
        return impl_.get<base::HostId>("console_host_id");

    return impl_.get<base::HostId>(sessionKeyForHostId(session_name));
}

//--------------------------------------------------------------------------------------------------
void HostKeyStorage::setLastHostId(const QString& session_name, base::HostId host_id)
{
    if (session_name.isEmpty())
        impl_.set<base::HostId>("console_host_id", host_id);
    else
        impl_.set<base::HostId>(sessionKeyForHostId(session_name), host_id);

    impl_.flush();
}

} // namespace host
