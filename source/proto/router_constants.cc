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

#include "proto/router_constants.h"

namespace proto::router {

const char* const kCommandHostDisconnect  = "disconnect";
const char* const kCommandHostRemove      = "remove";

const char* const kCommandRelayDisconnect = "disconnect";

const char* const kCommandClientDisconnect = "disconnect";

const char* const kCommandUserAdd         = "add";
const char* const kCommandUserModify      = "modify";
const char* const kCommandUserDelete      = "delete";

const char* const kCommandWorkspaceAdd    = "add";
const char* const kCommandWorkspaceModify = "modify";
const char* const kCommandWorkspaceDelete = "delete";

const char* const kErrorOk                = "ok";
const char* const kErrorInvalidRequest    = "invalid_request";
const char* const kErrorInternalError     = "internal_error";
const char* const kErrorInvalidEntryId    = "invalid_entry_id";
const char* const kErrorInvalidData       = "invalid_data";
const char* const kErrorAlreadyExists     = "already_exists";
const char* const kErrorNotFound          = "not_found";

const char* const kParamTryToUninstall    = "try_to_uninstall";

} // namespace proto::router
