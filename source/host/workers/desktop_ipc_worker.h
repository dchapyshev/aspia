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

#ifndef HOST_WORKERS_DESKTOP_IPC_WORKER_H
#define HOST_WORKERS_DESKTOP_IPC_WORKER_H

#include <QList>
#include <QPointer>
#include <QSize>

#include "base/scoped_qpointer.h"
#include "base/threading/worker.h"
#include "proto/desktop_control.h"
#include "proto/desktop_input.h"
#include "proto/desktop_internal.h"

namespace proto::clipboard {
class Event;
} // namespace proto::clipboard

namespace proto::screen {
class Screen;
} // namespace proto::screen

class DesktopAgentClient;
class IpcChannel;

// Runs the I/O side of the desktop agent: the control IPC channel to the service and the connected
// clients (protocol state, capability negotiation, config merge, overflow aggregation). Messages
// coming from the clients are forwarded to the media/input workers through signals; the serialized
// media they produce is fanned out to the clients through the on*Data slots. Owns nothing that
// captures or injects - only the wire.
class DesktopIpcWorker final : public Worker
{
    Q_OBJECT

public:
    DesktopIpcWorker();
    ~DesktopIpcWorker() final;

public slots:
    // Serialized messages produced by the media/input workers, fanned out to every client.
    void onVideoData(const QByteArray& buffer, bool is_key_frame);
    void onCursorShapeData(const QByteArray& buffer);
    void onCursorPositionData(const QByteArray& buffer);
    void onScreenListData(const QByteArray& buffer);
    void onScreenTypeData(const QByteArray& buffer);
    void onClipboardData(const QByteArray& buffer);
    void onAudioData(const QByteArray& buffer);

signals:
    // Forwarded from the clients to the input worker.
    void sig_injectKeyEvent(const proto::input::KeyEvent& event);
    void sig_injectTextEvent(const proto::input::TextEvent& event);
    void sig_injectMouseEvent(const proto::input::MouseEvent& event);
    void sig_injectTouchEvent(const proto::input::TouchEvent& event);

    // Forwarded from the clients to the screen worker.
    void sig_selectScreen(const proto::screen::Screen& screen);
    void sig_clipboardEvent(const proto::clipboard::Event& event);
    void sig_keyFrameRequested();
    void sig_preferredSizeChanged(const QSize& size);

    // Result of merging the configuration over all clients. |config| carries the merged wallpaper,
    // effects, cursor and preferred-resolution flags for the screen worker; the codec flags are the
    // client capability intersection (the host still gates hardware H264 itself).
    void sig_configure(const proto::control::Config& config,
                       bool vp8_supported, bool vp9_supported, bool h264_supported);

    // Derived from the merged configuration, targeting the audio and input workers.
    void sig_audioEnabled(bool enable);
    void sig_blockInput(bool enable);

    // Control commands from the service.
    void sig_paused(bool paused);
    void sig_mouseLocked(bool locked);
    void sig_keyboardLocked(bool locked);

    // The last client disconnected: capture must stop.
    void sig_stopCapture();

    void sig_overflowStateChanged(proto::desktop::Overflow::State state);
    void sig_bandwidthChanged(qint64 bandwidth);

protected:
    // Worker implementation.
    void onStart() final;
    void onStop() final;
    void onTimer(TimePoint now) final;

private slots:
    void onIpcConnected();
    void onIpcDisconnected();
    void onIpcErrorOccurred();
    void onIpcMessageReceived(quint32 ipc_channel_id, const QByteArray& buffer, bool reliable);
    void onClientConfigured();
    void onClientFinished();
    void onPreferredSizeChanged();
    void onClientBandwidthChanged();

private:
    void startClient(const QString& ipc_channel_name);
    void connectToService();
    qint64 minimalBandwidth() const;

    ScopedQPointer<IpcChannel> ipc_channel_;
    QList<DesktopAgentClient*> clients_;

    bool is_lock_at_disconnect_ = false;

    Q_DISABLE_COPY_MOVE(DesktopIpcWorker)
};

#endif // HOST_WORKERS_DESKTOP_IPC_WORKER_H
