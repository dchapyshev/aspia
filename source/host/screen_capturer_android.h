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

#ifndef HOST_SCREEN_CAPTURER_ANDROID_H
#define HOST_SCREEN_CAPTURER_ANDROID_H

#include <QMutex>
#include <QPoint>
#include <QRect>
#include <QSize>

#include "base/desktop/frame.h"
#include "host/screen_capturer.h"

// Screen capturer for Android, backed by the platform MediaProjection API (see ScreenCapturer.java).
// The Java side owns the MediaProjection, an ImageReader and a VirtualDisplay; every produced image is
// copied once, on the ImageReader listener thread, into a reused two-frame queue through the
// nativeOnFrame callback. captureFrame() hands the most recent frame to the capture thread without
// further allocation. MediaProjection exposes a single virtual display, so one screen is reported, and
// it delivers no separate cursor (the pointer is composited into the frames), so captureCursor()
// returns nullptr.
class ScreenCapturerAndroid final : public ScreenCapturer
{
    Q_OBJECT

public:
    ~ScreenCapturerAndroid() final;

    // Sets up the MediaProjection capture and starts delivering frames. Returns nullptr when the
    // projection could not be started (for example the user has not granted the screen capture
    // consent, or the required foreground service is not running).
    static ScreenCapturerAndroid* create(QObject* parent = nullptr);

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

protected:
    // ScreenCapturer implementation.
    void reset() final;

public:
    // Bridge entry points invoked from the JNI callbacks (see screen_capturer_android.cc), keyed to
    // this instance by |token_|. Not meant to be called directly.

    // Called from the nativeOnStarted callback on the Android main thread while start() waits: records
    // the virtual display geometry chosen by the Java side (or that the setup failed).
    void onStarted(bool success, const QSize& size, const QPoint& dpi);

    // Called from the nativeOnFrame callback on the ImageReader listener thread: copies |src|
    // (RGBA_8888, so the red and blue channels are swapped into the frame's native BGRA order) into the
    // current queue frame. |row_stride| is the source line pitch, which the reader may pad past
    // |width| * 4.
    void onFrame(const quint8* src, int width, int height, int row_stride);

private:
    explicit ScreenCapturerAndroid(QObject* parent);

    bool start();
    void stop();

    // Identifies this capturer to the Java side; frames and the start result carry it back.
    qint64 token_ = 0;
    bool active_ = false;

    // The virtual display geometry reported by start(); |screen_rect_| tracks the frame size and stays
    // valid before the first frame arrives so screenList() can report a resolution.
    QRect screen_rect_;
    QPoint dpi_ = QPoint(96, 96);

    // Default display geometry queried synchronously in start(), used to report a resolution before the
    // projection (which needs user consent) is running.
    QSize start_size_;
    QPoint start_dpi_ = QPoint(96, 96);

    // Two reused frame buffers guarded by |frame_mutex_|. The listener thread fills the current frame;
    // captureFrame() advances the queue so the producer never overwrites the frame being encoded.
    QMutex frame_mutex_;
    FrameQueue<Frame> queue_;
    bool has_new_frame_ = false;

    // Set (under |frame_mutex_|) when the user declined the screen capture consent; captureFrame() then
    // reports a permanent error so the caller stops instead of waiting for frames that never arrive.
    bool consent_failed_ = false;

    Q_DISABLE_COPY_MOVE(ScreenCapturerAndroid)
};

#endif // HOST_SCREEN_CAPTURER_ANDROID_H
