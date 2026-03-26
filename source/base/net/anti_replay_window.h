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

#ifndef BASE_NET_ANTI_REPLAY_WINDOW_H
#define BASE_NET_ANTI_REPLAY_WINDOW_H

#include <QtGlobal>

#include <bitset>

namespace base {

// Sliding window for detecting replayed or too-old packets.
// Based on the same approach used by WireGuard and RFC 6479.
//
// Usage:
//   AntiReplayWindow window;
//   if (!window.check(counter))
//       discard packet;  // replayed or too old
//
class AntiReplayWindow
{
public:
    static constexpr quint64 kWindowSize = 2048;

    AntiReplayWindow() = default;

    // Returns true if the counter is valid (not replayed and not too old).
    // If valid, marks the counter as seen.
    bool check(quint64 counter);

    // Resets the window to its initial state.
    void reset();

private:
    quint64 max_counter_ = 0;
    std::bitset<kWindowSize> bitmap_;

    Q_DISABLE_COPY_MOVE(AntiReplayWindow)
};

} // namespace base

#endif // BASE_NET_ANTI_REPLAY_WINDOW_H
