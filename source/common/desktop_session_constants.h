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

#ifndef COMMON_DESKTOP_SESSION_CONSTANTS_H
#define COMMON_DESKTOP_SESSION_CONSTANTS_H

#include <QtGlobal>

namespace common {

extern const char kSelectScreenExtension[];
extern const char kPreferredSizeExtension[];
extern const char kVideoRecordingExtension[];
extern const char kPowerControlExtension[];
extern const char kRemoteUpdateExtension[];
extern const char kSystemInfoExtension[];
extern const char kTaskManagerExtension[];
extern const char kVideoPauseExtension[];
extern const char kAudioPauseExtension[];
extern const char kScreenTypeExtension[];
extern const char kSwitchSessionExtension[];

extern const char kSupportedExtensionsForManage[];
extern const char kSupportedExtensionsForView[];

extern const quint32 kSupportedVideoEncodings;
extern const quint32 kSupportedAudioEncodings;

extern const char kFlagDisablePasteAsKeystrokes[];
extern const char kFlagDisableAudio[];
extern const char kFlagDisableClipboard[];
extern const char kFlagDisableCursorShape[];
extern const char kFlagDisableCursorPosition[];
extern const char kFlagDisableDesktopEffects[];
extern const char kFlagDisableDesktopWallpaper[];
extern const char kFlagDisableFontSmoothing[];
extern const char kFlagDisableClearClipboard[];
extern const char kFlagDisableLockAtDisconnect[];
extern const char kFlagDisableBlockInput[];

} // namespace common

#endif // COMMON_DESKTOP_SESSION_CONSTANTS_H
