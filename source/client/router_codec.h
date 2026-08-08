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

namespace proto::router {
class Group;
class GroupList;
class Host;
class HostList;
class HostSearchResult;
class TempHostList;
class Workspace;
} // namespace proto::router

// Translation between the wire records of the router and the structs the client works with. An
// outgoing record is measured against the protocol bounds before it is sent - an oversized reply
// is not refused by the router, it tears the session down. Nothing here holds state.

RouterHost decodeRouterHost(const proto::router::Host& src);
RouterHostList decodeRouterHostList(const proto::router::HostList& list);
RouterHostList decodeRouterHostSearchResult(const proto::router::HostSearchResult& result);
RouterGroupList decodeRouterGroupList(const proto::router::GroupList& list);
RouterTempHostList decodeRouterTempHostList(const proto::router::TempHostList& list);

// Fill the outgoing record from the plain struct. The returned error code is kErrorOk when the
// record can be sent; anything else is reported to the caller in the same terms the router would
// answer with, without being sent.
std::string_view buildRouterWorkspace(const RouterWorkspace& workspace,
                                      proto::router::Workspace* out);
std::string_view buildRouterHost(const RouterHost& host, proto::router::Host* out);
std::string_view buildRouterGroup(const RouterGroup& group, proto::router::Group* out);

#endif // CLIENT_ROUTER_CODEC_H
