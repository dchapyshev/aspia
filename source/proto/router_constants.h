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

// Page bounds of the list replies. A reply carrying a list of unbounded length grows past the
// message limit, and such a reply ends the session instead of being sent.
[[maybe_unused]] constexpr int kMaxHostPageSize = 100;
[[maybe_unused]] constexpr int kMaxTempHostPageSize = 100;
[[maybe_unused]] constexpr int kMaxUserPageSize = 100;

// Device tokens one user holds at a time. Issuing a token over the cap drops the least recently
// used ones, so a client that never comes back cannot grow the list without bound.
[[maybe_unused]] constexpr int kMaxDeviceTokensPerUser = 50;

// Size of a device token, in bytes. The router issues exactly this many and refuses any other
// size on presentation, so the client stores nothing else either.
[[maybe_unused]] constexpr int kDeviceTokenSize = 32;

// Bound on the blocked_seconds of a TwoFactorChallenge. The router sends the remaining part of
// a block that lasts minutes; the client clamps what it reads to [0, this], so a hostile peer
// cannot overflow the arithmetic on the value or park a record behind a forever block.
[[maybe_unused]] constexpr int64_t kMaxTwoFactorBlockSeconds = 24 * 60 * 60;

// Bound on the otpauth:// URI of an enrollment challenge, in UTF-8 bytes. The client is the side
// that checks it: it renders the URI into a QR code, and the encoder refuses a payload that does
// not fit its largest symbol, leaving the enrollment without an image to scan. What the router
// builds stays far below - the issuer, a user name bounded by User::kMaxUserNameLength and a
// Base32 secret come to some 700 bytes even when every character of the name is percent-encoded.
[[maybe_unused]] constexpr size_t kMaxOtpauthUriLength = 1024;

// Bounds on the manager-editable fields of a host, a group and a workspace, in UTF-8 bytes.
// Checked on both sides. An unbounded record grows the list reply that carries it past the message
// limit, and such a reply ends the session instead of being sent, on every reconnect.
[[maybe_unused]] constexpr size_t kMaxEntryNameLength = 64;
[[maybe_unused]] constexpr size_t kMaxCommentLength = 8 * 1024;

// Command names for HostRequest.
extern const char* const kCommandHostDisconnect;
extern const char* const kCommandHostRemove;
extern const char* const kCommandHostModify;
extern const char* const kCommandHostUpdate;
extern const char* const kCommandHostApprove;

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

// Error codes for HostResult/RelayResult/ClientResult/UserList/UserResult/WorkspaceList/
// WorkspaceResult/HostIdResponse.
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

} // namespace proto::router

#endif // PROTO_ROUTER_CONSTANTS_H
