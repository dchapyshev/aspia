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

extern const char kSelectScreenExtension[]; // Deprecated.
extern const char kPreferredSizeExtension[]; // Deprecated.
extern const char kPowerControlExtension[]; // Deprecated.
extern const char kSystemInfoExtension[]; // Deprecated.
extern const char kTaskManagerExtension[]; // Deprecated.
extern const char kVideoPauseExtension[]; // Deprecated.
extern const char kAudioPauseExtension[]; // Deprecated.
extern const char kScreenTypeExtension[]; // Deprecated.

extern const char kFlagDisablePasteAsKeystrokes[]; // Deprecated.
extern const char kFlagDisableAudio[]; // Deprecated.
extern const char kFlagDisableClipboard[]; // Deprecated.
extern const char kFlagDisableCursorShape[]; // Deprecated.
extern const char kFlagDisableCursorPosition[]; // Deprecated.
extern const char kFlagDisableDesktopEffects[]; // Deprecated.
extern const char kFlagDisableDesktopWallpaper[]; // Deprecated.
extern const char kFlagDisableLockAtDisconnect[]; // Deprecated.
extern const char kFlagDisableBlockInput[]; // Deprecated.

extern const quint32 kSupportedVideoEncodings;
extern const quint32 kSupportedAudioEncodings;

extern const char kFlagPasteAsKeystrokes[];
extern const char kFlagAudio[];
extern const char kFlagClipboard[];
extern const char kFlagCursorShape[];
extern const char kFlagCursorPosition[];
extern const char kFlagDesktopEffects[];
extern const char kFlagDesktopWallpaper[];
extern const char kFlagLockAtDisconnect[];
extern const char kFlagBlockInput[];
extern const char kFlagPowerControl[];
extern const char kFlagSelectScreen[];
extern const char kFlagSystemInfo[];
extern const char kFlagTaskManager[];

} // namespace common

#endif // COMMON_DESKTOP_SESSION_CONSTANTS_H
