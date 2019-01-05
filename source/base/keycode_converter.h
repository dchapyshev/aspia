//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef ASPIA_BASE__KEYCODE_CONVERTER_H
#define ASPIA_BASE__KEYCODE_CONVERTER_H

#include <cstdint>

#include "base/macros_magic.h"

namespace aspia {

// This structure is used to define the keycode mapping table.
// It is defined here because the unittests need access to it.
typedef struct
{
    // USB keycode:
    //  Upper 16-bits: USB Usage Page.
    //  Lower 16-bits: USB Usage Id: Assigned ID within this usage page.
    uint32_t usb_keycode;

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
    static int invalidNativeKeycode();

    // Return the value that identifies an invalid USB keycode.
    static uint32_t invalidUsbKeycode();

    // Return the value that identifies an invalid Qt keycode.
    static int invalidQtKeycode();

    // Convert a USB keycode into an equivalent platform native keycode.
    static int usbKeycodeToNativeKeycode(uint32_t usb_keycode);

    // Convert a platform native keycode into an equivalent USB keycode.
    static uint32_t nativeKeycodeToUsbKeycode(int native_keycode);

    // Convert a Qt keycode into an equivalent USB keycode.
    static uint32_t qtKeycodeToUsbKeycode(int qt_keycode);

private:
    DISALLOW_COPY_AND_ASSIGN(KeycodeConverter);
};

} // namespace aspia

#endif // ASPIA_BASE__KEYCODE_CONVERTER_H
