//
// PROJECT:         Aspia
// FILE:            protocol/keycode_converter.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
// NOTE:            This file is based on Chromium code.
//

#include "protocol/keycode_converter.h"

namespace aspia {

namespace {

#if defined(Q_OS_WIN)
#define USB_KEYMAP(usb, evdev, xkb, win, mac, qt) {usb, win, qt}
#elif defined(Q_OS_LINUX)
#define USB_KEYMAP(usb, evdev, xkb, win, mac, qt) {usb, xkb, qt}
#elif defined(Q_OS_MACOS)
#define USB_KEYMAP(usb, evdev, xkb, win, mac, qt) {usb, mac, qt}
#elif defined(Q_OS_ANDROID)
#define USB_KEYMAP(usb, evdev, xkb, win, mac, qt) {usb, evdev, qt}
#else
#define USB_KEYMAP(usb, evdev, xkb, win, mac, qt) {usb, 0, qt}
#endif
#define USB_KEYMAP_DECLARATION const KeycodeMapEntry usb_keycode_map[] =
#include "protocol/keycode_converter_data.inc"
#undef USB_KEYMAP
#undef USB_KEYMAP_DECLARATION

const size_t kKeycodeMapEntries = _countof(usb_keycode_map);

} // namespace

// static
int KeycodeConverter::InvalidNativeKeycode()
{
    return usb_keycode_map[0].native_keycode;
}

// static
quint32 KeycodeConverter::InvalidUsbKeycode()
{
    return usb_keycode_map[0].usb_keycode;
}

// static
int KeycodeConverter::InvalidQtKeycode()
{
    return usb_keycode_map[0].qt_keycode;
}

// static
int KeycodeConverter::UsbKeycodeToNativeKeycode(quint32 usb_keycode)
{
    // Deal with some special-cases that don't fit the 1:1 mapping.
    if (usb_keycode == 0x070032) // non-US hash.
        usb_keycode = 0x070031; // US backslash.
#if defined(Q_OS_MACOS)
    if (usb_keycode == 0x070046) // PrintScreen.
        usb_keycode = 0x070068; // F13.
#endif

    for (size_t i = 0; i < kKeycodeMapEntries; ++i)
    {
        if (usb_keycode_map[i].usb_keycode == usb_keycode)
            return usb_keycode_map[i].native_keycode;
    }

    return InvalidNativeKeycode();
}

// static
quint32 KeycodeConverter::NativeKeycodeToUsbKeycode(int native_keycode)
{
    for (size_t i = 0; i < kKeycodeMapEntries; ++i)
    {
        if (usb_keycode_map[i].native_keycode == native_keycode)
            return usb_keycode_map[i].usb_keycode;
    }

    return InvalidUsbKeycode();
}

// static
int KeycodeConverter::QtKeycodeToUsbKeycode(int qt_keycode)
{
    for (size_t i = 0; i < kKeycodeMapEntries; ++i)
    {
        if (usb_keycode_map[i].qt_keycode == qt_keycode)
            return usb_keycode_map[i].usb_keycode;
    }

    return InvalidUsbKeycode();
}

} // namespace aspia
