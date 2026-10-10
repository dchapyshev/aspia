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

#ifndef HOST_WORKERS_PORTABLE_DESKTOP_WORKER_H
#define HOST_WORKERS_PORTABLE_DESKTOP_WORKER_H

#include <QList>
#include <QSize>

#include "base/threading/worker.h"
#include "proto/desktop_control.h"
#include "proto/desktop_input.h"
#include "proto/desktop_internal.h"

namespace proto::screen {
class Screen;
} // namespace proto::screen

class DesktopAgentClient;

// Desktop side of the portable host running with the rights of the user.
class PortableDesktopWorker final : public Worker
{
    Q_OBJECT

public:
    PortableDesktopWorker();
    ~PortableDesktopWorker() final;

public slots:
    void onVideoData(const QByteArray& buffer, bool is_key_frame);
    void onCursorShapeData(const QByteArray& buffer);
    void onCursorPositionData(const QByteArray& buffer);
    void onScreenListData(const QByteArray& buffer);
    void onScreenTypeData(const QByteArray& buffer);
    void onAudioData(const QByteArray& buffer);

    void onClientStarted(quint32 client_id);
    void onClientMessage(quint32 client_id, quint32 channel_id, const QByteArray& buffer);
    void onClientFinished(quint32 client_id);
    void onClientChannelChanged();

    void onUserPause(bool enable);
    void onUserLockMouse(bool enable);
    void onUserLockKeyboard(bool enable);

signals:
    void sig_clientMessage(quint32 client_id, quint32 channel_id, const QByteArray& buffer, bool reliable);

    // Forwarded from the clients to the input worker.
    void sig_injectKeyEvent(const proto::input::KeyEvent& event);
    void sig_injectTextEvent(const proto::input::TextEvent& event);
    void sig_injectMouseEvent(const proto::input::MouseEvent& event);
    void sig_injectTouchEvent(const proto::input::TouchEvent& event);

    // Forwarded from the clients to the screen worker.
    void sig_selectScreen(const proto::screen::Screen& screen);
    void sig_keyFrameRequested();
    void sig_preferredSizeChanged(const QSize& size);

    // Result of merging the configuration over all clients.
    void sig_configure(const proto::control::Config& config, bool vp8_supported, bool vp9_supported);

    // Derived from the merged configuration, targeting the audio worker.
    void sig_audioEnabled(bool enable);

    // Commands of the user.
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
    void onClientConfigured();
    void onPreferredSizeChanged();
    void onClientBandwidthChanged();

private:
    struct ClientEntry
    {
        quint32 id = 0;
        DesktopAgentClient* client = nullptr;
    };

    qsizetype indexOfClient(quint32 client_id) const;
    void lastClientFinished();
    qint64 minimalBandwidth() const;

    QList<ClientEntry> clients_;

    Q_DISABLE_COPY_MOVE(PortableDesktopWorker)
};

#endif // HOST_WORKERS_PORTABLE_DESKTOP_WORKER_H
