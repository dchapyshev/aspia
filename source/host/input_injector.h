//
// PROJECT:         Aspia
// FILE:            host/input_injector.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_HOST__INPUT_INJECTOR_H
#define _ASPIA_HOST__INPUT_INJECTOR_H

#include <QPoint>
#include <QSet>

#include "desktop_capture/scoped_thread_desktop.h"
#include "protocol/desktop_session.pb.h"

namespace aspia {

class InputInjector
{
public:
    InputInjector() = default;
    ~InputInjector();

    void injectPointerEvent(const proto::desktop::PointerEvent& event);
    void injectKeyEvent(const proto::desktop::KeyEvent& event);

private:
    void switchToInputDesktop();
    bool isCtrlAndAltPressed();

    ScopedThreadDesktop desktop_;

    QSet<quint32> pressed_keys_;

    QPoint prev_mouse_pos_;
    quint32 prev_mouse_button_mask_ = 0;

    Q_DISABLE_COPY(InputInjector)
};

} // namespace aspia

#endif // _ASPIA_HOST__INPUT_INJECTOR_H
