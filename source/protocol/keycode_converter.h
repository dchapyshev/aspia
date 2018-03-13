//
// PROJECT:         Aspia
// FILE:            protocol/keycode_converter.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
// NOTE:            This file is based on Chromium code.
//

#ifndef _ASPIA_PROTOCOL__KEYCODE_CONVERTER_H
#define _ASPIA_PROTOCOL__KEYCODE_CONVERTER_H

#include <QtCore>

#include "base/macros.h"

namespace aspia {

// This structure is used to define the keycode mapping table.
// It is defined here because the unittests need access to it.
typedef struct
{
    // USB keycode:
    //  Upper 16-bits: USB Usage Page.
    //  Lower 16-bits: USB Usage Id: Assigned ID within this usage page.
    quint32 usb_keycode;

    // Contains one of the following:
    //  On Linux: XKB scancode
    //  On Windows: Windows OEM scancode
    //  On Mac: Mac keycode
    int native_keycode;

    int qt_keycode;
} KeycodeMapEntry;

class KeycodeConverter
{
public:
    // Return the value that identifies an invalid native keycode.
    static int InvalidNativeKeycode();

    // Return the value that identifies an invalid USB keycode.
    static quint32 InvalidUsbKeycode();

    // Return the value that identifies an invalid Qt keycode.
    static int InvalidQtKeycode();

    // Convert a USB keycode into an equivalent platform native keycode.
    static int UsbKeycodeToNativeKeycode(quint32 usb_keycode);

    // Convert a platform native keycode into an equivalent USB keycode.
    static quint32 NativeKeycodeToUsbKeycode(int native_keycode);

    // Convert a Qt keycode into an equivalent USB keycode.
    static int QtKeycodeToUsbKeycode(int qt_keycode);

private:
    DISALLOW_COPY_AND_ASSIGN(KeycodeConverter);
};

} // namespace aspia

#endif // _ASPIA_PROTOCOL__KEYCODE_CONVERTER_H
