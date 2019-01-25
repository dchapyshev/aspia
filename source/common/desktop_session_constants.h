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

#ifndef COMMON__DESKTOP_SESSION_CONSTANTS_H
#define COMMON__DESKTOP_SESSION_CONSTANTS_H

#include <cstdint>

namespace common {

extern const char kSelectScreenExtension[];
extern const char kPowerControlExtension[];
extern const char kRemoteUpdateExtension[];
extern const char kSystemInfoExtension[];

extern const char kSupportedExtensionsForManage[];
extern const char kSupportedExtensionsForView[];

extern const uint32_t kSupportedVideoEncodings;

} // namespace common

#endif // COMMON__DESKTOP_SESSION_CONSTANTS_H
