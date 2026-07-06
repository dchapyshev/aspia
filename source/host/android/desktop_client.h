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

#ifndef HOST_ANDROID_DESKTOP_CLIENT_H
#define HOST_ANDROID_DESKTOP_CLIENT_H

#include <QSize>

#include "host/client.h"

namespace proto::clipboard {
class Event;
} // namespace proto::clipboard

namespace proto::input {
class KeyEvent;
class MouseEvent;
class TextEvent;
class TouchEvent;
} // namespace proto::input

class DesktopClient final : public Client
{
    Q_OBJECT

public:
    explicit DesktopClient(TcpChannel* tcp_channel, QObject* parent = nullptr);
    ~DesktopClient() final;

    bool isConfigured() const { return configured_; }
    bool isVp8Supported() const { return vp8_supported_; }
    bool isVp9Supported() const { return vp9_supported_; }
    const QSize& preferredSize() const { return preferred_size_; }

    // Forward shared session data (encoded once by the DesktopAgent) to this client's channel.
    void onVideoData(const QByteArray& buffer, bool is_key_frame);
    void onScreenListData(const QByteArray& buffer);
    void onClipboardData(const QByteArray& buffer);

signals:
    void sig_configured();
    void sig_keyFrameRequested();
    void sig_preferredSizeChanged();
    void sig_screenListRequested();
    void sig_injectKeyEvent(const proto::input::KeyEvent& event);
    void sig_injectTextEvent(const proto::input::TextEvent& event);
    void sig_injectMouseEvent(const proto::input::MouseEvent& event);
    void sig_injectTouchEvent(const proto::input::TouchEvent& event);
    void sig_injectClipboardEvent(const proto::clipboard::Event& event);

protected:
    // Client implementation.
    void onStart() final;
    void onMessage(quint8 channel_id, const QByteArray& buffer) final;

private:
    void handleControl(const QByteArray& buffer);
    void handleVideoControl(const QByteArray& buffer);
    void handleScreenControl(const QByteArray& buffer);
    void handleInput(const QByteArray& buffer);
    void handleClipboard(const QByteArray& buffer);
    void sendCapabilities();

    // Codecs the connected client advertised; the agent uses them to pick a common encoding.
    bool vp8_supported_ = false;
    bool vp9_supported_ = false;

    // Set once the client has sent its config; the agent captures only for configured clients.
    bool configured_ = false;

    // Video is muted for this client while it is paused; the agent keeps encoding for the others.
    bool is_paused_ = false;

    QSize preferred_size_;

    Q_DISABLE_COPY_MOVE(DesktopClient)
};

#endif // HOST_ANDROID_DESKTOP_CLIENT_H
