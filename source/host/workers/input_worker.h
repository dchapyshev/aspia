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

#ifndef HOST_WORKERS_INPUT_WORKER_H
#define HOST_WORKERS_INPUT_WORKER_H

#include <QPoint>
#include <QPointer>
#include <QSize>

#include "base/scoped_qpointer.h"
#include "base/threading/worker.h"
#include "host/screen_capturer.h"

namespace proto::input {
class KeyEvent;
class MouseEvent;
class TextEvent;
class TouchEvent;
} // namespace proto::input

class InputInjector;
class IpcWorker;
class ScreenWorker;

// Injects the input received from clients into the user session. All input enters here (gating and
// coordinate scaling), so a busy video pipeline never delays keystrokes and pointer motion. The
// worker owns the platform injector on Windows and macOS, and on Linux for the X11 and kernel-level
// (KMS/KWin/wlroots) captures. For the VT and Wayland captures the injector is tied to
// capturer-owned resources and lives in ScreenWorker, so those events are delegated to it. Which
// case applies is decided from the capture type reported through ScreenWorker::sig_screenInfoChanged.
class InputWorker final : public Worker
{
    Q_OBJECT

public:
    InputWorker();
    ~InputWorker() final;

public slots:
    void onInjectKeyEvent(const proto::input::KeyEvent& event);
    void onInjectTextEvent(const proto::input::TextEvent& event);
    void onInjectMouseEvent(const proto::input::MouseEvent& event);
    void onInjectTouchEvent(const proto::input::TouchEvent& event);

    void onSetPaused(bool paused);
    void onSetMouseLocked(bool locked);
    void onSetKeyboardLocked(bool locked);
    void onSetBlockInput(bool enable);

protected:
    // Worker implementation.
    void onStart() final;
    void onStop() final;

private slots:
    void onScaleFactorChanged(double scale_x, double scale_y);
    void onScreenInfoChanged(ScreenCapturer::Type type, const QSize& screen_size, const QPoint& offset);

private:
    // Creates the injector matching the capture backend |type|, or releases it when injection is
    // delegated to ScreenWorker (Linux VT/Wayland). Several capture types map to the same injector
    // (WIN_GDI and WIN_DXGI both use InputInjectorWin), so it is recreated only when the injector's
    // type() would actually change.
    void updateInjectorForCaptureType(ScreenCapturer::Type type);

    // Owned on Windows/macOS, and on Linux for the X11/uinput captures. Null before the first
    // captured frame and when injection is delegated to ScreenWorker (Linux VT/Wayland).
    ScopedQPointer<InputInjector> input_injector_;

    // Delegation target for platforms where the injector belongs to the capture path. Resolved
    // through WorkerManager when the worker starts.
    QPointer<ScreenWorker> screen_worker_;

    // Source of the client input and gating commands. Resolved through WorkerManager on start.
    QPointer<IpcWorker> ipc_worker_;

    bool is_paused_ = false;
    bool is_mouse_locked_ = false;
    bool is_keyboard_locked_ = false;

    // Video pipeline state published by ScreenWorker; zero until the first encoded frame, mouse
    // events are dropped meanwhile (there is nothing to map them onto yet).
    double scale_x_ = 0.0;
    double scale_y_ = 0.0;
    QSize screen_size_;
    QPoint screen_offset_;

    Q_DISABLE_COPY_MOVE(InputWorker)
};

#endif // HOST_WORKERS_INPUT_WORKER_H
