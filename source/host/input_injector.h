//
// PROJECT:         Aspia
// FILE:            host/input_injector.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_HOST__INPUT_INJECTOR_H
#define _ASPIA_HOST__INPUT_INJECTOR_H

#include <QThread>

#include <condition_variable>
#include <queue>
#include <mutex>
#include <variant>

#include "protocol/desktop_session.pb.h"

namespace aspia {

class InputInjector : public QThread
{
    Q_OBJECT

public:
    InputInjector(QObject* parent);
    ~InputInjector();

    void injectPointerEvent(const proto::desktop::PointerEvent& event);
    void injectKeyEvent(const proto::desktop::KeyEvent& event);

protected:
    // QThread implementation.
    void run() override;

private:
    using InputEvent = std::variant<proto::desktop::PointerEvent, proto::desktop::KeyEvent>;

    std::condition_variable input_event_;
    std::mutex input_queue_lock_;
    std::queue<InputEvent> incoming_input_queue_;
    bool terminate_ = false;

    Q_DISABLE_COPY(InputInjector)
};

} // namespace aspia

#endif // _ASPIA_HOST__INPUT_INJECTOR_H
