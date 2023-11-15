//
// Aspia Project
// Copyright (C) 2016-2023 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "host/stat_counter.h"

#include "base/logging.h"

namespace host {

//--------------------------------------------------------------------------------------------------
StatCounter::StatCounter(uint32_t client_session_id, std::shared_ptr<base::TaskRunner> task_runner)
    : client_session_id_(client_session_id),
    timer_(base::WaitableTimer::Type::REPEATED, std::move(task_runner))
{
    timer_.start(std::chrono::seconds(30), std::bind(&StatCounter::onTimeout, this));
}

//--------------------------------------------------------------------------------------------------
StatCounter::~StatCounter()
{
    timer_.stop();
}

//--------------------------------------------------------------------------------------------------
void StatCounter::addVideoPacket()
{
    ++video_packets_;
}

//--------------------------------------------------------------------------------------------------
void StatCounter::addAudioPacket()
{
    ++audio_packets_;
}

//--------------------------------------------------------------------------------------------------
void StatCounter::addIncomingClipboardEvent()
{
    ++incoming_clipboard_events_;
}

//--------------------------------------------------------------------------------------------------
void StatCounter::addOutgoingClipboardEvent()
{
    ++outgoing_clipboard_events_;
}

//--------------------------------------------------------------------------------------------------
void StatCounter::addKeyboardEvent()
{
    ++keyboard_events_;
}

//--------------------------------------------------------------------------------------------------
void StatCounter::addTextEvent()
{
    ++text_events_;
}

//--------------------------------------------------------------------------------------------------
void StatCounter::addMouseEvent()
{
    ++mouse_events_;
}

//--------------------------------------------------------------------------------------------------
void StatCounter::addVideoError()
{
    ++video_error_count_;
}

//--------------------------------------------------------------------------------------------------
void StatCounter::addCursorPosition()
{
    ++cursor_positions_;
}

//--------------------------------------------------------------------------------------------------
void StatCounter::onTimeout()
{
    LOG(LS_INFO) << "### Statistics (sid=" << client_session_id_ << "###";
    LOG(LS_INFO) << "Packets: video=" << video_packets_ << " audio=" << audio_packets_
                 << " video_error=" << video_error_count_;
    LOG(LS_INFO) << "Clipboard: in=" << incoming_clipboard_events_
                 << " out=" << outgoing_clipboard_events_;
    LOG(LS_INFO) << "Input: keyboard=" << keyboard_events_ << " mouse=" << mouse_events_
                 << " text=" << text_events_;
    LOG(LS_INFO) << "Cursor positions: " << cursor_positions_;
}

} // namespace host
