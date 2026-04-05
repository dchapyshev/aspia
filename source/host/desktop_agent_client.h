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

#ifndef HOST_DESKTOP_AGENT_CLIENT_H
#define HOST_DESKTOP_AGENT_CLIENT_H

#include <QObject>

#include <optional>

#include "base/logging.h"
#include "proto/desktop_audio.h"
#include "proto/desktop_control.h"
#include "proto/desktop_input.h"
#include "proto/desktop_internal.h"
#include "proto/desktop_power.h"
#include "proto/desktop_screen.h"
#include "proto/desktop_video.h"

namespace base {
class IpcChannel;
} // namespace base

namespace host {

class DesktopAgentClient final : public QObject
{
    Q_OBJECT

public:
    explicit DesktopAgentClient(QObject* parent = nullptr);
    ~DesktopAgentClient() final;

    proto::peer::SessionType sessionType() const { return session_type_; }
    proto::desktop::Overflow::State overflowState() const { return overflow_state_; }
    const QSize& preferredSize() const { return preferred_size_; }
    qint64 bandwidth() const { return bandwidth_; }
    std::optional<proto::control::Config> config() const { return config_; }

    bool isVp8Supported() const { return vp8_supported_; }
    bool isVp9Supported() const { return vp9_supported_; }
    bool isOpusSupported() const { return opus_supported_; }
    bool isFileClipboardSupported() const { return file_clipboard_supported_; }

    void onVideoData(const QByteArray& buffer, bool is_key_frame);
    void onScreenListData(const QByteArray& buffer);
    void onScreenTypeData(const QByteArray& buffer);
    void onCursorPositionData(const QByteArray& buffer);
    void onCursorShapeData(const QByteArray& buffer);
    void onAudioData(const QByteArray& buffer);

public slots:
    void start(const QString& ipc_channel_name);

signals:
    void sig_injectKeyEvent(const proto::input::KeyEvent& event);
    void sig_injectTextEvent(const proto::input::TextEvent& event);
    void sig_injectMouseEvent(const proto::input::MouseEvent& event);
    void sig_injectTouchEvent(const proto::input::TouchEvent& event);
    void sig_selectScreen(const proto::screen::Screen& screen);
    void sig_preferredSizeChanged(const QSize& size);
    void sig_keyFrameRequested();
    void sig_configured();
    void sig_finished();

private slots:
    void onIpcConnected();
    void onIpcErrorOccurred();
    void onIpcDisconnected();
    void onIpcMessageReceived(quint32 channel_id, const QByteArray& buffer, bool reliable);

private:
    void readSessionMessage(quint8 channel_id, const QByteArray& buffer);
    void sendSessionMessage(quint8 channel_id, const QByteArray& buffer, bool reliable);

    void readPreferredSize(const proto::video::PreferredSize& size);
    void readVideoPause(const proto::video::Pause& pause);
    void readAudioPause(const proto::audio::Pause& pause);
    void readCapabilities(const proto::control::Capabilities& capabilities);
    void readConfig(const proto::control::Config& config);
    void readPowerControl(const proto::power::Control& control);
    void readOverflow(proto::desktop::Overflow::State state);
    void sendCapabilities();

    base::IpcChannel* ipc_channel_ = nullptr;

    proto::desktop::Overflow::State overflow_state_ = proto::desktop::Overflow::STATE_NONE;
    proto::peer::SessionType session_type_ = proto::peer::SESSION_TYPE_UNKNOWN;
    std::optional<proto::control::Config> config_;

    bool vp8_supported_ = false;
    bool vp9_supported_ = false;
    bool opus_supported_ = false;
    bool file_clipboard_supported_ = false;

    QSize preferred_size_;
    qint64 bandwidth_ = 0;

    bool is_video_paused_ = false;
    bool is_audio_paused_ = false;

    LOG_DECLARE_CONTEXT(DesktopAgentClient);
    Q_DISABLE_COPY_MOVE(DesktopAgentClient)
};

} // namespace host

#endif // HOST_DESKTOP_AGENT_CLIENT_H
