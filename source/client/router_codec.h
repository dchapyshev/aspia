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

#ifndef CLIENT_ROUTER_CODEC_H
#define CLIENT_ROUTER_CODEC_H

#include <string_view>

#include "client/router_types.h"

class DataCryptor;
class RouterKeys;

namespace proto::router {
class Group;
class GroupList;
class Host;
class HostList;
class HostSearchResult;
class TempHostList;
class Workspace;
} // namespace proto::router

// Translation between the wire records of the router and the plain structs the client works with.
// The encrypted fields are opened and sealed with the workspace keys borrowed from RouterKeys, and
// an outgoing record is measured against the protocol bounds before it is sent - an oversized reply
// is not refused by the router, it tears the session down. Nothing here holds state.

// One field, encrypted with the group key of its workspace. An empty value stays empty; a failed
// encryption yields nothing at all, because an empty result would overwrite the stored value.
QString decryptField(const DataCryptor& cryptor, std::string_view ciphertext);
std::optional<QByteArray> encryptField(const DataCryptor& cryptor, const QString& plaintext);

// The encrypted fields of a host stay empty when we hold no key of its workspace; the plain ones
// are filled either way.
RouterHost decodeRouterHost(const RouterKeys& keys, const proto::router::Host& src);

RouterHostList decodeRouterHostList(const RouterKeys& keys, const proto::router::HostList& list);
RouterHostList decodeRouterHostSearchResult(const RouterKeys& keys,
                                            const proto::router::HostSearchResult& result);
RouterGroupList decodeRouterGroupList(const RouterKeys& keys, const proto::router::GroupList& list);
RouterTempHostList decodeRouterTempHostList(const proto::router::TempHostList& list);

// Fill the outgoing record from the plain struct. The returned error code is kErrorOk when the
// record can be sent; anything else is reported to the caller in the same terms the router would
// answer with, without being sent.
std::string_view buildRouterWorkspace(const RouterKeys& keys, const RouterWorkspace& workspace,
                                      proto::router::Workspace* out);
std::string_view buildRouterHost(const RouterKeys& keys, const RouterHost& host,
                                 proto::router::Host* out);
std::string_view buildRouterGroup(const RouterKeys& keys, qint64 workspace_id,
                                  const RouterGroup& group, proto::router::Group* out);

#endif // CLIENT_ROUTER_CODEC_H
