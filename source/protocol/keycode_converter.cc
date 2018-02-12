//
// PROJECT:         Aspia
// FILE:            protocol/keycode_converter.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "protocol/keycode_converter.h"

#include <cstdlib>

namespace aspia {

namespace {

// This structure is used to define the keycode mapping table.
// It is defined here because the unittests need access to it.
typedef struct
{
    // USB keycode:
    //  Upper 16-bits: USB Usage Page.
    //  Lower 16-bits: USB Usage Id: Assigned ID within this usage page.
    uint32_t usb_keycode;

    // Windows OEM scancode
    int native_keycode;
} KeycodeMapEntry;

const KeycodeMapEntry usb_keycode_map[] =
{
    // USB      Win
    { 0x000000, 0x0000 }, // Invalid

    // =========================================
    // USB Usage Page 0x01: Generic Desktop Page
    // =========================================

    { 0x010082, 0xe05f }, // System Sleep
    { 0x010083, 0xe063 }, // Wake Up

    // =========================================
    // USB Usage Page 0x07: Keyboard/Keypad Page
    // =========================================

    { 0x070001, 0x00ff },
    { 0x070002, 0x00fc },
    { 0x070004, 0x001e }, // aA
    { 0x070005, 0x0030 }, // bB
    { 0x070006, 0x002e }, // cC
    { 0x070007, 0x0020 }, // dD
    { 0x070008, 0x0012 }, // eE
    { 0x070009, 0x0021 }, // fF
    { 0x07000a, 0x0022 }, // gG
    { 0x07000b, 0x0023 }, // hH
    { 0x07000c, 0x0017 }, // iI
    { 0x07000d, 0x0024 }, // jJ
    { 0x07000e, 0x0025 }, // kK
    { 0x07000f, 0x0026 }, // lL
    { 0x070010, 0x0032 }, // mM
    { 0x070011, 0x0031 }, // nN
    { 0x070012, 0x0018 }, // oO
    { 0x070013, 0x0019 }, // pP
    { 0x070014, 0x0010 }, // qQ
    { 0x070015, 0x0013 }, // rR
    { 0x070016, 0x001f }, // sS
    { 0x070017, 0x0014 }, // tT
    { 0x070018, 0x0016 }, // uU
    { 0x070019, 0x002f }, // vV
    { 0x07001a, 0x0011 }, // wW
    { 0x07001b, 0x002d }, // xX
    { 0x07001c, 0x0015 }, // yY
    { 0x07001d, 0x002c }, // zZ
    { 0x07001e, 0x0002 }, // 1!
    { 0x07001f, 0x0003 }, // 2@
    { 0x070020, 0x0004 }, // 3#
    { 0x070021, 0x0005 }, // 4$
    { 0x070022, 0x0006 }, // 5%
    { 0x070023, 0x0007 }, // 6^
    { 0x070024, 0x0008 }, // 7&
    { 0x070025, 0x0009 }, // 8*
    { 0x070026, 0x000a }, // 9(
    { 0x070027, 0x000b }, // 0)
    { 0x070028, 0x001c }, // Enter
    { 0x070029, 0x0001 }, // Escape
    { 0x07002a, 0x000e }, // Backspace
    { 0x07002b, 0x000f }, // Tab
    { 0x07002c, 0x0039 }, // Spacebar
    { 0x07002d, 0x000c }, // -_
    { 0x07002e, 0x000d }, // =+
    { 0x07002f, 0x001a }, // Bracket Left
    { 0x070030, 0x001b }, // Bracket Right
    { 0x070031, 0x002b }, // \|
    { 0x070033, 0x0027 }, // ;:
    { 0x070034, 0x0028 }, // '"
    { 0x070035, 0x0029 }, // `~
    { 0x070036, 0x0033 }, // ,<
    { 0x070037, 0x0034 }, // .>
    { 0x070038, 0x0035 }, // /?
    { 0x070039, 0x003a }, // Caps Lock
    { 0x07003a, 0x003b }, // F1
    { 0x07003b, 0x003c }, // F2
    { 0x07003c, 0x003d }, // F3
    { 0x07003d, 0x003e }, // F4
    { 0x07003e, 0x003f }, // F5
    { 0x07003f, 0x0040 }, // F6
    { 0x070040, 0x0041 }, // F7
    { 0x070041, 0x0042 }, // F8
    { 0x070042, 0x0043 }, // F9
    { 0x070043, 0x0044 }, // F10
    { 0x070044, 0x0057 }, // F11
    { 0x070045, 0x0058 }, // F12
    { 0x070046, 0xe037 }, // Print Screen
    { 0x070047, 0x0046 }, // Scroll Lock
    { 0x070048, 0x0045 }, // Pause
    { 0x070049, 0xe052 }, // Insert
    { 0x07004a, 0xe047 }, // Home
    { 0x07004b, 0xe049 }, // Page Up
    { 0x07004c, 0xe053 }, // Del
    { 0x07004d, 0xe04f }, // End
    { 0x07004e, 0xe051 }, // Page Down
    { 0x07004f, 0xe04d }, // Arrow Right
    { 0x070050, 0xe04b }, // Arrow Left
    { 0x070051, 0xe050 }, // Arrow Down
    { 0x070052, 0xe048 }, // Arrow Up
    { 0x070053, 0xe045 }, // Num Lock
    { 0x070054, 0xe035 }, // Num Pad Divide
    { 0x070055, 0x0037 }, // Keypad_*
    { 0x070056, 0x004a }, // Keypad_-
    { 0x070057, 0x004e }, // Num Pad Add
    { 0x070058, 0xe01c }, // Num Pad Enter
    { 0x070059, 0x004f }, // Num Pad 1 + End
    { 0x07005a, 0x0050 }, // Num Pad 2 + Down
    { 0x07005b, 0x0051 }, // Num Pad 3 + PageDn
    { 0x07005c, 0x004b }, // Num Pad 4 + Left
    { 0x07005d, 0x004c }, // Num Pad 5
    { 0x07005e, 0x004d }, // Num Pad 6 + Right
    { 0x07005f, 0x0047 }, // Num Pad 7 + Home
    { 0x070060, 0x0048 }, // Num Pad + Up
    { 0x070061, 0x0049 }, // Num Pad + PageUp
    { 0x070062, 0x0052 }, // Num Pad + Insert
    { 0x070063, 0x0053 },  // Keypad_. Delete
    { 0x070064, 0x0056 }, // Intl Backslash
    { 0x070065, 0xe05d }, // Context Menu
    { 0x070066, 0xe05e }, // Power
    { 0x070067, 0x0059 }, // Numpad Equal
    { 0x070068, 0x0064 }, // F13
    { 0x070069, 0x0065 }, // F14
    { 0x07006a, 0x0066 }, // F15
    { 0x07006b, 0x0067 }, // F16
    { 0x07006c, 0x0068 }, // F17
    { 0x07006d, 0x0069 }, // F18
    { 0x07006e, 0x006a }, // F19
    { 0x07006f, 0x006b }, // F20
    { 0x070070, 0x006c }, // F21
    { 0x070071, 0x006d }, // F22
    { 0x070072, 0x006e }, // F23
    { 0x070073, 0x00c2 }, // F24
    { 0x070075, 0xe03b }, // Help
    { 0x07007a, 0xe008 }, // Undo
    { 0x07007b, 0xe017 }, // Cut
    { 0x07007c, 0xe018 }, // Copy
    { 0x07007d, 0xe00a }, // Paste
    { 0x07007f, 0xe020 }, // Volume Mute
    { 0x070080, 0xe030 }, // Volume Up
    { 0x070081, 0xe02e }, // Volume Down
    { 0x070085, 0x007e }, // Num Pad Comma

    // International1
    { 0x070087, 0x0073 }, // Intl Ro
    { 0x070088, 0x0070 }, // KanaMode
    { 0x070089, 0x007d }, // IntlYen
    { 0x07008a, 0x0079 }, // Convert
    { 0x07008b, 0x007b }, // NonConvert
    { 0x070090, 0x0072 }, // Lang1
    { 0x070091, 0x0071 }, // Lang2
    { 0x070092, 0x0078 }, // Lang3
    { 0x070093, 0x0077 }, // Lang4
    { 0x0700e0, 0x001d }, // ControlLeft
    { 0x0700e1, 0x002a }, // ShiftLeft
    { 0x0700e2, 0x0038 }, // AltLeft
    { 0x0700e3, 0xe05b }, // MetaLeft
    { 0x0700e4, 0xe01d }, // ControlRight
    { 0x0700e5, 0x0036 }, // ShiftRight
    { 0x0700e6, 0xe038 }, // AltRight
    { 0x0700e7, 0xe05c }, // MetaRight

    // ==================================
    // USB Usage Page 0x0c: Consumer Page
    // ==================================

    { 0x0c00b5, 0xe019 }, // MediaTrackNext
    { 0x0c00b6, 0x00a5 }, // MediaTrackPrevious
    { 0x0c00b7, 0xe024 }, // MediaStop
    { 0x0c00b8, 0xe02c }, // Eject
    { 0x0c00cd, 0xe022 }, // MediaPlayPause
    { 0x0c0183, 0xe06d }, // MediaSelect
    { 0x0c018a, 0xe06c }, // LaunchMail
    { 0x0c0192, 0xe021 }, // LaunchApp2
    { 0x0c0194, 0xe06b }, // LaunchApp1
    { 0x0c0221, 0xe065 }, // BrowserSearch
    { 0x0c0223, 0xe032 }, // BrowserHome
    { 0x0c0224, 0xe06a }, // BrowserBack
    { 0x0c0225, 0xe069 }, // BrowserForward
    { 0x0c0226, 0xe068 }, // BrowserStop
    { 0x0c0227, 0xe067 }, // BrowserRefresh
    { 0x0c022a, 0xe066 }  // BrowserFavorites
};

const size_t kKeycodeMapEntries = _countof(usb_keycode_map);

} // namespace

// static
int KeycodeConverter::InvalidNativeKeycode()
{
    return usb_keycode_map[0].native_keycode;
}

// static
uint32_t KeycodeConverter::InvalidUsbKeycode()
{
    return usb_keycode_map[0].usb_keycode;
}

// static
int KeycodeConverter::UsbKeycodeToNativeKeycode(uint32_t usb_keycode)
{
    // Deal with some special-cases that don't fit the 1:1 mapping.
    if (usb_keycode == 0x070032) // non-US hash.
        usb_keycode = 0x070031; // US backslash.

    for (size_t i = 0; i < kKeycodeMapEntries; ++i)
    {
        if (usb_keycode_map[i].usb_keycode == usb_keycode)
            return usb_keycode_map[i].native_keycode;
    }

    return InvalidNativeKeycode();
}

// static
uint32_t KeycodeConverter::NativeKeycodeToUsbKeycode(int native_keycode)
{
    for (size_t i = 0; i < kKeycodeMapEntries; ++i)
    {
        if (usb_keycode_map[i].native_keycode == native_keycode)
            return usb_keycode_map[i].usb_keycode;
    }

    return InvalidUsbKeycode();
}

} // namespace aspia
