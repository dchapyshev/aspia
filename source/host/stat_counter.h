//
// SmartCafe Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include <QTimer>

namespace host {

class StatCounter final : public QObject
{
    Q_OBJECT

public:
    explicit StatCounter(quint32 client_session_id, QObject* parent = nullptr);
    ~StatCounter();

    void addVideoPacket();
    void addAudioPacket();
    void addIncomingClipboardEvent();
    void addOutgoingClipboardEvent();
    void addKeyboardEvent();
    void addTextEvent();
    void addMouseEvent();
    void addTouchEvent();
    void addVideoError();
    void addCursorPosition();

private slots:
    void onTimeout();

private:
    const quint32 client_session_id_;

    QTimer timer_;

    quint64 video_packets_ = 0;
    quint64 audio_packets_ = 0;
    quint64 incoming_clipboard_events_ = 0;
    quint64 outgoing_clipboard_events_ = 0;
    quint64 keyboard_events_ = 0;
    quint64 text_events_ = 0;
    quint64 mouse_events_ = 0;
    quint64 touch_events_ = 0;
    quint64 video_error_count_ = 0;
    quint64 cursor_positions_ = 0;

    Q_DISABLE_COPY(StatCounter)
};

} // namespace host

#endif // HOST_STAT_COUNTER_H
