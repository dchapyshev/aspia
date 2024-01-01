//
// Aspia Project
// Copyright (C) 2016-2024 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef HOST_STAT_COUNTER_H
#define HOST_STAT_COUNTER_H

#include "base/macros_magic.h"
#include "base/waitable_timer.h"

#include <cstdint>

namespace host {

class StatCounter
{
public:
    StatCounter(uint32_t client_session_id, std::shared_ptr<base::TaskRunner> task_runner);
    ~StatCounter();

    void addVideoPacket();
    void addAudioPacket();
    void addIncomingClipboardEvent();
    void addOutgoingClipboardEvent();
    void addKeyboardEvent();
    void addTextEvent();
    void addMouseEvent();
    void addVideoError();
    void addCursorPosition();

private:
    void onTimeout();

    const uint32_t client_session_id_;

    base::WaitableTimer timer_;

    uint64_t video_packets_ = 0;
    uint64_t audio_packets_ = 0;
    uint64_t incoming_clipboard_events_ = 0;
    uint64_t outgoing_clipboard_events_ = 0;
    uint64_t keyboard_events_ = 0;
    uint64_t text_events_ = 0;
    uint64_t mouse_events_ = 0;
    uint64_t video_error_count_ = 0;
    uint64_t cursor_positions_ = 0;

    DISALLOW_COPY_AND_ASSIGN(StatCounter);
};

} // namespace host

#endif // HOST_STAT_COUNTER_H
