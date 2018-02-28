//
// PROJECT:         Aspia
// FILE:            protocol/keycode_converter.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_PROTOCOL__KEYCODE_CONVERTER_H
#define _ASPIA_PROTOCOL__KEYCODE_CONVERTER_H

#include <cstdint>

#include "base/macros.h"

namespace aspia {

class KeycodeConverter
{
public:
    // Return the value that identifies an invalid native keycode.
    static int InvalidNativeKeycode();

    // Return the value that identifies an invalid USB keycode.
    static uint32_t InvalidUsbKeycode();

    // Convert a USB keycode into an equivalent platform native keycode.
    static int UsbKeycodeToNativeKeycode(uint32_t usb_keycode);

    // Convert a platform native keycode into an equivalent USB keycode.
    static uint32_t NativeKeycodeToUsbKeycode(int native_keycode);

private:
    DISALLOW_COPY_AND_ASSIGN(KeycodeConverter);
};

} // namespace aspia

#endif // _ASPIA_PROTOCOL__KEYCODE_CONVERTER_H
