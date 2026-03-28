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

#include "common/desktop_session_constants.h"

#include "proto/desktop_audio.h"
#include "proto/desktop_screen.h"

namespace common {

const char kSelectScreenExtension[] = "select_screen";
const char kPreferredSizeExtension[] = "preferred_size";
const char kPowerControlExtension[] = "power_control";
const char kSystemInfoExtension[] = "system_info";
const char kTaskManagerExtension[] = "task_manager";
const char kVideoPauseExtension[] = "video_pause";
const char kAudioPauseExtension[] = "audio_pause";
const char kScreenTypeExtension[] = "screen_type";

#if defined(Q_OS_WINDOWS)
const char kSupportedExtensionsForManage[] = "select_screen;power_control;system_info;task_manager";
const char kSupportedExtensionsForView[] = "select_screen;system_info";
#else
const char kSupportedExtensionsForManage[] = "select_screen";
const char kSupportedExtensionsForView[] = "select_screen";
#endif

const quint32 kSupportedVideoEncodings =
    proto::desktop::VIDEO_ENCODING_VP8 | proto::desktop::VIDEO_ENCODING_VP9;
const quint32 kSupportedAudioEncodings = proto::desktop::AUDIO_ENCODING_OPUS;

const char kFlagDisablePasteAsKeystrokes[] = "disable_paste_as_keystrokes";
const char kFlagDisableAudio[] = "disable_audio";
const char kFlagDisableClipboard[] = "disable_clipboard";
const char kFlagDisableCursorShape[] = "disable_cursor_shape";
const char kFlagDisableCursorPosition[] = "disable_cursor_position";
const char kFlagDisableDesktopEffects[] = "disable_desktop_effects";
const char kFlagDisableDesktopWallpaper[] = "disable_desktop_wallpaper";
const char kFlagDisableLockAtDisconnect[] = "disable_lock_at_disconnect";
const char kFlagDisableBlockInput[] = "disable_block_input";

} // namespace common
