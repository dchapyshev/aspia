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

#include <cstddef>
#include <cstdint>

namespace proto::router {

// Page bounds of the list replies.
[[maybe_unused]] constexpr int kMaxClientPageSize = 100;
[[maybe_unused]] constexpr int kMaxHostPageSize = 100;
[[maybe_unused]] constexpr int kMaxTempHostPageSize = 100;
[[maybe_unused]] constexpr int kMaxUserPageSize = 100;
[[maybe_unused]] constexpr int kMaxAccessLogPageSize = 100;

// Bound on the workspaces of a HostFilter.
[[maybe_unused]] constexpr int kMaxHostFilterWorkspaces = 100;

// Bounds on the text of a HostFilter and of a host search query, in characters.
[[maybe_unused]] constexpr size_t kMaxHostQueryLength = 100;
[[maybe_unused]] constexpr size_t kMaxHostIdLength = 20;

// Bound on each text of an AccessLogFilter, in characters.
[[maybe_unused]] constexpr size_t kMaxAccessLogQueryLength = 100;

// Device tokens one user holds at a time. A token over the cap drops the least recently used one.
[[maybe_unused]] constexpr int kMaxDeviceTokensPerUser = 50;

// Size of a device token, in bytes.
[[maybe_unused]] constexpr int kDeviceTokenSize = 32;

// Bound on the blocked_seconds of a TwoFactorChallenge.
[[maybe_unused]] constexpr int64_t kMaxTwoFactorBlockSeconds = 24 * 60 * 60;

// Bound on the otpauth:// URI of an enrollment challenge, in UTF-8 bytes.
[[maybe_unused]] constexpr size_t kMaxOtpauthUriLength = 1024;

// Bounds on the manager-editable fields of a host, a group and a workspace, in characters.
[[maybe_unused]] constexpr size_t kMaxEntryNameLength = 64;
[[maybe_unused]] constexpr size_t kMaxCommentLength = 2048;

// Version of the JSON the host sends in HostTelemetry.
[[maybe_unused]] constexpr int kTelemetryVersion = 1;

// Command names for HostRequest.
extern const char* const kCommandHostDisconnect;
extern const char* const kCommandHostRemove;
extern const char* const kCommandHostModify;
extern const char* const kCommandHostUpdate;
extern const char* const kCommandHostApprove;
extern const char* const kCommandHostTelemetry;

// Command names for RelayRequest.
extern const char* const kCommandRelayDisconnect;

// Command names for ClientRequest.
extern const char* const kCommandClientDisconnect;

// Command names for UserRequest.
extern const char* const kCommandUserAdd;
extern const char* const kCommandUserModify;
extern const char* const kCommandUserDelete;
extern const char* const kCommandUserResetOtp;

// Command names for UserTokenRequest.
extern const char* const kCommandUserTokenRevoke;

// Command names for WorkspaceRequest.
extern const char* const kCommandWorkspaceAdd;
extern const char* const kCommandWorkspaceModify;
extern const char* const kCommandWorkspaceDelete;

// Command names for GroupRequest.
extern const char* const kCommandGroupAdd;
extern const char* const kCommandGroupModify;
extern const char* const kCommandGroupDelete;

// Command names for PeerRequest.
extern const char* const kCommandPeerDisconnect;

// Error codes of the router results.
extern const char* const kErrorOk;
extern const char* const kErrorInvalidRequest;
extern const char* const kErrorInternalError;
extern const char* const kErrorInvalidEntryId;
extern const char* const kErrorInvalidData;
extern const char* const kErrorAlreadyExists;
extern const char* const kErrorNotFound;
extern const char* const kErrorAccessDenied;
extern const char* const kErrorConflict;
extern const char* const kErrorLostConnection;
extern const char* const kErrorHostOffline;
extern const char* const kErrorKeyPoolEmpty;
extern const char* const kErrorTooManyRequests;

} // namespace proto::router

#endif // PROTO_ROUTER_CONSTANTS_H
