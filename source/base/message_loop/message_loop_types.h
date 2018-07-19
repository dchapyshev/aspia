//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef ASPIA_BASE__MESSAGE_LOOP__MESSAGE_LOOP_TYPES_H_
#define ASPIA_BASE__MESSAGE_LOOP__MESSAGE_LOOP_TYPES_H_

#include <chrono>

namespace aspia {

using MessageLoopClock = std::chrono::high_resolution_clock;
using MessageLoopTimePoint = std::chrono::time_point<MessageLoopClock>;

} // namespace aspia

#endif // ASPIA_BASE__MESSAGE_LOOP__MESSAGE_LOOP_TYPES_H_
