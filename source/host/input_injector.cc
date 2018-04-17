//
// PROJECT:         Aspia
// FILE:            host/input_injector.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/input_injector.h"

#include <QPoint>
#include <QRect>
#include <QSet>
#include <QSettings>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <sas.h>

#include "base/system_error_code.h"
#include "base/keycode_converter.h"
#include "desktop_capture/scoped_thread_desktop.h"

namespace aspia {

namespace {

const char kSoftwareSASGenerationPath[] =
    "HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System";

const char kSoftwareSASGeneration[] = "SoftwareSASGeneration";

const quint32 kUsbCodeDelete = 0x07004c;
const quint32 kUsbCodeLeftCtrl = 0x0700e0;
const quint32 kUsbCodeRightCtrl = 0x0700e4;
const quint32 kUsbCodeLeftAlt = 0x0700e2;
const quint32 kUsbCodeRightAlt = 0x0700e6;

void sendKeyboardScancode(WORD scancode, DWORD flags)
{
    INPUT input;
    memset(&input, 0, sizeof(input));

    input.type       = INPUT_KEYBOARD;
    input.ki.dwFlags = flags;
    input.ki.wScan   = scancode;

    if (!(flags & KEYEVENTF_UNICODE))
    {
        input.ki.wScan &= 0xFF;

        if ((scancode & 0xFF00) != 0x0000)
            input.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
    }

    // Do the keyboard event.
    if (!SendInput(1, &input, sizeof(input)))
        qWarning() << "SendInput failed: " << lastSystemErrorString();
}

void sendKeyboardVirtualKey(WORD key_code, DWORD flags)
{
    INPUT input;
    memset(&input, 0, sizeof(input));

    input.type       = INPUT_KEYBOARD;
    input.ki.wVk     = key_code;
    input.ki.dwFlags = flags;
    input.ki.wScan   = static_cast<WORD>(MapVirtualKeyW(key_code, MAPVK_VK_TO_VSC));

    // Do the keyboard event.
    if (!SendInput(1, &input, sizeof(input)))
        qWarning() << "SendInput failed: " << lastSystemErrorString();
}

class InputInjectorImpl
{
public:
    InputInjectorImpl() = default;
    ~InputInjectorImpl();

    void injectPointerEvent(const proto::desktop::PointerEvent& event);
    void injectKeyEvent(const proto::desktop::KeyEvent& event);

private:
    void switchToInputDesktop();
    bool isCtrlAndAltPressed();

    ScopedThreadDesktop desktop_;
    QSet<quint32> pressed_keys_;
    QPoint prev_mouse_pos_;
    quint32 prev_mouse_button_mask_ = 0;

    Q_DISABLE_COPY(InputInjectorImpl)
};

InputInjectorImpl::~InputInjectorImpl()
{
    for (auto usb_keycode : pressed_keys_)
    {
        int scancode = KeycodeConverter::UsbKeycodeToNativeKeycode(usb_keycode);
        if (scancode != KeycodeConverter::InvalidNativeKeycode())
        {
            sendKeyboardScancode(static_cast<WORD>(scancode),
                                 KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP);
        }
    }
}

void InputInjectorImpl::injectPointerEvent(const proto::desktop::PointerEvent& event)
{
    switchToInputDesktop();

    QRect screen_rect = QRect(GetSystemMetrics(SM_XVIRTUALSCREEN),
                              GetSystemMetrics(SM_YVIRTUALSCREEN),
                              GetSystemMetrics(SM_CXVIRTUALSCREEN),
                              GetSystemMetrics(SM_CYVIRTUALSCREEN));
    if (!screen_rect.contains(event.x(), event.y()))
        return;

    // Translate the coordinates of the cursor into the coordinates of the virtual screen.
    QPoint pos(((event.x() - screen_rect.x()) * 65535) / (screen_rect.width() - 1),
        ((event.y() - screen_rect.y()) * 65535) / (screen_rect.height() - 1));

    DWORD flags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK;
    DWORD wheel_movement = 0;

    if (pos != prev_mouse_pos_)
    {
        flags |= MOUSEEVENTF_MOVE;
        prev_mouse_pos_ = pos;
    }

    quint32 mask = event.mask();

    bool prev = (prev_mouse_button_mask_ & proto::desktop::PointerEvent::LEFT_BUTTON) != 0;
    bool curr = (mask & proto::desktop::PointerEvent::LEFT_BUTTON) != 0;

    if (curr != prev)
    {
        flags |= (curr ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP);
    }

    prev = (prev_mouse_button_mask_ & proto::desktop::PointerEvent::MIDDLE_BUTTON) != 0;
    curr = (mask & proto::desktop::PointerEvent::MIDDLE_BUTTON) != 0;

    if (curr != prev)
    {
        flags |= (curr ? MOUSEEVENTF_MIDDLEDOWN : MOUSEEVENTF_MIDDLEUP);
    }

    prev = (prev_mouse_button_mask_ & proto::desktop::PointerEvent::RIGHT_BUTTON) != 0;
    curr = (mask & proto::desktop::PointerEvent::RIGHT_BUTTON) != 0;

    if (curr != prev)
    {
        flags |= (curr ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP);
    }

    if (mask & proto::desktop::PointerEvent::WHEEL_UP)
    {
        flags |= MOUSEEVENTF_WHEEL;
        wheel_movement = static_cast<DWORD>(WHEEL_DELTA);
    }
    else if (mask & proto::desktop::PointerEvent::WHEEL_DOWN)
    {
        flags |= MOUSEEVENTF_WHEEL;
        wheel_movement = static_cast<DWORD>(-WHEEL_DELTA);
    }

    INPUT input;
    memset(&input, 0, sizeof(input));

    input.type         = INPUT_MOUSE;
    input.mi.dx        = pos.x();
    input.mi.dy        = pos.y();
    input.mi.mouseData = wheel_movement;
    input.mi.dwFlags   = flags;

    // Do the mouse event.
    if (!SendInput(1, &input, sizeof(input)))
        qWarning() << "SendInput failed: " << lastSystemErrorString();

    prev_mouse_button_mask_ = mask;
}

void InputInjectorImpl::injectKeyEvent(const proto::desktop::KeyEvent& event)
{
    if (event.flags() & proto::desktop::KeyEvent::PRESSED)
    {
        pressed_keys_.insert(event.usb_keycode());

        if (event.usb_keycode() == kUsbCodeDelete && isCtrlAndAltPressed())
        {
            QSettings settings(kSoftwareSASGenerationPath, QSettings::Registry64Format);

            static const int kNone = 0;
            static const int kApplications = 2;

            int old_state = settings.value(kSoftwareSASGeneration, kNone).toInt();
            if (old_state < kApplications)
                settings.setValue(kSoftwareSASGeneration, kApplications);

            SendSAS(FALSE);
            settings.setValue(kSoftwareSASGeneration, old_state);
            return;
        }
    }
    else
    {
        pressed_keys_.remove(event.usb_keycode());
    }

    int scancode = KeycodeConverter::UsbKeycodeToNativeKeycode(event.usb_keycode());
    if (scancode == KeycodeConverter::InvalidNativeKeycode())
        return;

    switchToInputDesktop();

    bool prev_state = GetKeyState(VK_CAPITAL) != 0;
    bool curr_state = (event.flags() & proto::desktop::KeyEvent::CAPSLOCK) != 0;

    if (prev_state != curr_state)
    {
        sendKeyboardVirtualKey(VK_CAPITAL, 0);
        sendKeyboardVirtualKey(VK_CAPITAL, KEYEVENTF_KEYUP);
    }

    prev_state = GetKeyState(VK_NUMLOCK) != 0;
    curr_state = (event.flags() & proto::desktop::KeyEvent::NUMLOCK) != 0;

    if (prev_state != curr_state)
    {
        sendKeyboardVirtualKey(VK_NUMLOCK, 0);
        sendKeyboardVirtualKey(VK_NUMLOCK, KEYEVENTF_KEYUP);
    }

    DWORD flags = KEYEVENTF_SCANCODE;

    if (!(event.flags() & proto::desktop::KeyEvent::PRESSED))
        flags |= KEYEVENTF_KEYUP;

    sendKeyboardScancode(static_cast<WORD>(scancode), flags);
}

void InputInjectorImpl::switchToInputDesktop()
{
    Desktop input_desktop(Desktop::inputDesktop());

    if (input_desktop.isValid() && !desktop_.isSame(input_desktop))
    {
        desktop_.setThreadDesktop(std::move(input_desktop));
    }

    // We send a notification to the system that it is used to prevent
    // the screen saver, going into hibernation mode, etc.
    SetThreadExecutionState(ES_SYSTEM_REQUIRED);
}

bool InputInjectorImpl::isCtrlAndAltPressed()
{
    bool ctrl_pressed = false;
    bool alt_pressed = false;

    for (const auto& key : pressed_keys_)
    {
        if (key == kUsbCodeLeftCtrl || key == kUsbCodeRightCtrl)
            ctrl_pressed = true;

        if (key == kUsbCodeLeftAlt || key == kUsbCodeRightAlt)
            alt_pressed = true;
    }

    return ctrl_pressed && alt_pressed;
}

} // namespace

InputInjector::InputInjector(QObject* parent)
    : QThread(parent)
{
    start(QThread::HighPriority);
}

InputInjector::~InputInjector()
{
    {
        std::scoped_lock<std::mutex> lock(input_queue_lock_);
        terminate_ = true;
        input_event_.notify_one();
    }

    wait();
}

void InputInjector::injectPointerEvent(const proto::desktop::PointerEvent& event)
{
    std::scoped_lock<std::mutex> lock(input_queue_lock_);
    incoming_input_queue_.emplace(event);
    input_event_.notify_one();
}

void InputInjector::injectKeyEvent(const proto::desktop::KeyEvent& event)
{
    std::scoped_lock<std::mutex> lock(input_queue_lock_);
    incoming_input_queue_.emplace(event);
    input_event_.notify_one();
}

void InputInjector::run()
{
    InputInjectorImpl impl;

    while (true)
    {
        std::queue<InputEvent> work_input_queue;

        {
            std::unique_lock<std::mutex> lock(input_queue_lock_);

            while (incoming_input_queue_.empty() && !terminate_)
                input_event_.wait(lock);

            if (terminate_)
                return;

            work_input_queue.swap(incoming_input_queue_);
        }

        while (!work_input_queue.empty())
        {
            const InputEvent& input_event = work_input_queue.front();

            if (std::holds_alternative<proto::desktop::KeyEvent>(input_event))
            {
                impl.injectKeyEvent(std::get<proto::desktop::KeyEvent>(input_event));
            }
            else if (std::holds_alternative<proto::desktop::PointerEvent>(input_event))
            {
                impl.injectPointerEvent(std::get<proto::desktop::PointerEvent>(input_event));
            }

            work_input_queue.pop();
        }
    }
}

} // namespace aspia
