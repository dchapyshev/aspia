//
// Aspia Project
// Copyright (C) 2019 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "common/desktop_session_constants.h"
#include "proto/desktop_session.pb.h"

namespace common {

const char kSelectScreenExtension[] = "select_screen";
const char kPowerControlExtension[] = "power_control";
const char kRemoteUpdateExtension[] = "remote_update";
const char kSystemInfoExtension[] = "system_info";

const char kSupportedExtensionsForManage[] =
    "select_screen;power_control;remote_update;system_info";

const char kSupportedExtensionsForView[] =
    "select_screen;system_info";

const uint32_t kSupportedVideoEncodings =
    proto::desktop::VIDEO_ENCODING_VP8 | proto::desktop::VIDEO_ENCODING_VP9 |
    proto::desktop::VIDEO_ENCODING_ZSTD;

} // namespace common
