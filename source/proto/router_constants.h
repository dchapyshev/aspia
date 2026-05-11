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

#ifndef PROTO_ROUTER_CONSTANTS_H
#define PROTO_ROUTER_CONSTANTS_H

namespace proto::router {

// Command names for HostRequest.
extern const char* const kCommandHostDisconnect;
extern const char* const kCommandHostRemove;

// Command names for RelayRequest.
extern const char* const kCommandRelayDisconnect;

// Command names for UserRequest.
extern const char* const kCommandUserAdd;
extern const char* const kCommandUserModify;
extern const char* const kCommandUserDelete;

// Error codes for HostResult/RelayResult/UserList/UserResult.
extern const char* const kErrorOk;
extern const char* const kErrorInvalidRequest;
extern const char* const kErrorInternalError;
extern const char* const kErrorInvalidEntryId;
extern const char* const kErrorInvalidData;
extern const char* const kErrorAlreadyExists;

// Values for HostRequest::params.
extern const char* const kParamTryToUninstall;

} // namespace proto::router

#endif // PROTO_ROUTER_CONSTANTS_H
