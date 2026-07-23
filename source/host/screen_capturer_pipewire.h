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

#ifndef HOST_SCREEN_CAPTURER_PIPEWIRE_H
#define HOST_SCREEN_CAPTURER_PIPEWIRE_H

#include <QByteArray>
#include <QMutex>
#include <QPoint>
#include <QPointer>
#include <QRect>
#include <QSize>

#include <sys/types.h>

#include <atomic>
#include <memory>

#include "base/scoped_qpointer.h"
#include "base/time_types.h"
#include "base/desktop/frame.h"
#include "base/desktop/region.h"
#include "host/screen_capturer.h"

struct pw_context;
struct pw_core;
struct pw_stream;
struct pw_thread_loop;
struct spa_hook;
struct spa_pod;
struct spa_pod_builder;
struct PipeWireApi;

class EglDmaBuf;
class MouseCursor;
class WaylandCompositorSource;

// Screen capturer for Wayland sessions. It owns the compositor capture source (Mutter ScreenCast or the
// xdg-desktop-portal session), negotiates it asynchronously, and drives the resulting PipeWire stream.
// PipeWire delivers frames on its own thread loop; they are copied once into a reused two-frame queue
// and handed to the capture thread without further allocation. A single monitor stream is exposed, so
// one screen is reported. The cursor arrives as separate metadata (cursor_mode METADATA), not baked
// into frames. The source also injects input, which is routed in through the inject* slots rather than
// by sharing the source object with the input injector.
class ScreenCapturerPipeWire final : public ScreenCapturer
{
    Q_OBJECT

public:
    explicit ScreenCapturerPipeWire(uid_t session_uid, QObject* parent = nullptr);
    ~ScreenCapturerPipeWire() final;

    // Creates the capturer and begins negotiating a compositor capture source; the stream becomes
    // available asynchronously (sig_started reports the outcome). Returns nullptr if no compositor
    // source backend is available.
    static ScreenCapturerPipeWire* create(uid_t session_uid, QObject* parent = nullptr);

    // ScreenCapturer implementation.
    int screenCount() final;
    bool screenList(ScreenList* screens) final;
    bool selectScreen(ScreenId screen_id) final;
    ScreenId currentScreen() const final;
    const Frame* captureFrame(Error* error) final;
    const MouseCursor* captureCursor() final;
    QPoint cursorPosition() final;
    const QRect& desktopRect() const final;
    const QRect& currentScreenRect() const final;

    // Flags the stream for a restart from the PipeWire state-changed callback (called on the PipeWire
    // thread). captureFrame() then triggers an internal re-negotiation on the capture thread.
    void requestRestart();

    // Records the paused/unpaused stream state from the PipeWire state-changed callback (called on the
    // PipeWire thread). captureFrame() falls back to KMS if the stream stays paused (a locked session).
    void setStreamPaused(bool paused);

    // The compositor source backing the stream (it is also the input target). Valid after
    // sig_started(true); the input injector is built on it and shares it with the capturer.
    WaylandCompositorSource* compositorSource() const;

signals:
    // The capture source finished negotiating its stream. On success the stream is live; on failure the
    // owner should fall back to another capture path.
    void sig_started(bool success);

protected:
    // ScreenCapturer implementation.
    void reset() final;

private slots:
    // The compositor source finished negotiating; builds the PipeWire stream (or reports failure).
    void onSourceStarted(bool success);
    // Re-negotiates the source after a stream error (queued from captureFrame so it runs off the
    // capture call stack).
    void onRestartSource();

private:
    bool init();
    bool startStream();
    void stopStream();

    // Builds the EnumFormat parameters; offers DMA-BUF formats only when |allow_dmabuf| is set.
    int buildFormatParams(spa_pod_builder* builder, const spa_pod** params, bool allow_dmabuf);
    // Re-negotiates the stream without DMA-BUF after an EGL import failure (falls back to shm).
    void disableDmaBufAndRenegotiate();

    // PipeWire stream callbacks (called on the PipeWire thread loop).
    static void onParamChanged(void* data, quint32 id, const spa_pod* param);
    static void onProcess(void* data);
    void handleParamChanged(quint32 id, const spa_pod* param);
    void handleProcess();

    ScopedQPointer<WaylandCompositorSource> source_;

    const PipeWireApi* api_ = nullptr;
    pw_thread_loop* thread_loop_ = nullptr;
    pw_context* context_ = nullptr;
    pw_core* core_ = nullptr;
    pw_stream* stream_ = nullptr;
    std::unique_ptr<spa_hook> stream_listener_;

    // Set from the PipeWire state-changed callback when the stream errors, consumed on the capture
    // thread to re-create the stream.
    std::atomic<bool> restart_requested_ = false;

    // Set from the PipeWire state-changed callback to mirror the paused state. Read on the capture
    // thread by captureFrame(), which measures how long it stays paused (|paused_since_|) and, past a
    // grace period, falls back to KMS - the compositor pauses screencast on a locked session, so a
    // prolonged pause means the lock screen is only reachable below the compositor. |fallback_requested_|
    // makes that fire once.
    std::atomic<bool> stream_paused_ = false;
    TimePoint paused_since_;
    bool fallback_requested_ = false;

    // Imports DMA-BUF frames via EGL/GBM when the compositor delivers them.
    std::unique_ptr<EglDmaBuf> egl_dmabuf_;

    // Negotiated format; touched only on the PipeWire thread (param_changed and process callbacks
    // run on the same loop).
    QSize format_size_;
    bool swap_red_blue_ = false;
    bool dmabuf_disabled_ = false;
    quint32 format_fourcc_ = 0;
    quint64 format_modifier_ = 0;

    // Two reused frame buffers guarded by |frame_mutex_|. The PipeWire thread always fills the
    // current frame; captureFrame() advances the queue so the producer never overwrites the frame
    // being encoded.
    QMutex frame_mutex_;
    FrameQueue<Frame> queue_;
    bool has_new_frame_ = false;
    QRect screen_rect_;

    // Damage accumulated across produced-but-not-yet-captured frames; |full_damage_| forces a full
    // frame when no damage information is available or the size changed.
    Region accumulated_damage_;
    bool full_damage_ = true;

    // Cursor delivered as separate metadata, guarded by |frame_mutex_|.
    QByteArray cursor_data_;
    QSize cursor_size_;
    QPoint cursor_hotspot_;
    QPoint cursor_position_;
    bool cursor_changed_ = false;
    std::unique_ptr<MouseCursor> current_cursor_;

    Q_DISABLE_COPY(ScreenCapturerPipeWire)
};

#endif // HOST_SCREEN_CAPTURER_PIPEWIRE_H
