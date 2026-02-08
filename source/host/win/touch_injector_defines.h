//
// SmartCafe Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef HOST_WIN_TOUCH_INJECTOR_DEFINES_H
#define HOST_WIN_TOUCH_INJECTOR_DEFINES_H

#include <qt_windows.h>

typedef DWORD OWN_POINTER_INPUT_TYPE;
typedef UINT32 OWN_POINTER_FLAGS;

typedef enum
{
    PT_POINTER = 1,
    PT_TOUCH = 2,
    PT_PEN = 3,
    PT_MOUSE = 4,
    PT_TOUCHPAD = 5
} OWN_POINTER_FLAGS_ENUM;

#define OWN_POINTER_FLAG_NONE               0x00000000 // Default
#define OWN_POINTER_FLAG_NEW                0x00000001 // New pointer
#define OWN_POINTER_FLAG_INRANGE            0x00000002 // Pointer has not departed
#define OWN_POINTER_FLAG_INCONTACT          0x00000004 // Pointer is in contact
#define OWN_POINTER_FLAG_FIRSTBUTTON        0x00000010 // Primary action
#define OWN_POINTER_FLAG_SECONDBUTTON       0x00000020 // Secondary action
#define OWN_POINTER_FLAG_THIRDBUTTON        0x00000040 // Third button
#define OWN_POINTER_FLAG_FOURTHBUTTON       0x00000080 // Fourth button
#define OWN_POINTER_FLAG_FIFTHBUTTON        0x00000100 // Fifth button
#define OWN_POINTER_FLAG_PRIMARY            0x00002000 // Pointer is primary
#define OWN_POINTER_FLAG_CONFIDENCE         0x00004000 // Pointer is considered unlikely to be accidental
#define OWN_POINTER_FLAG_CANCELED           0x00008000 // Pointer is departing in an abnormal manner
#define OWN_POINTER_FLAG_DOWN               0x00010000 // Pointer transitioned to down state (made contact)
#define OWN_POINTER_FLAG_UPDATE             0x00020000 // Pointer update
#define OWN_POINTER_FLAG_UP                 0x00040000 // Pointer transitioned from down state (broke contact)
#define OWN_POINTER_FLAG_WHEEL              0x00080000 // Vertical wheel
#define OWN_POINTER_FLAG_HWHEEL             0x00100000 // Horizontal wheel
#define OWN_POINTER_FLAG_CAPTURECHANGED     0x00200000 // Lost capture
#define OWN_POINTER_FLAG_HASTRANSFORM       0x00400000 // Input has a transform associated with it

typedef enum
{
    POINTER_CHANGE_NONE,
    POINTER_CHANGE_FIRSTBUTTON_DOWN,
    POINTER_CHANGE_FIRSTBUTTON_UP,
    POINTER_CHANGE_SECONDBUTTON_DOWN,
    POINTER_CHANGE_SECONDBUTTON_UP,
    POINTER_CHANGE_THIRDBUTTON_DOWN,
    POINTER_CHANGE_THIRDBUTTON_UP,
    POINTER_CHANGE_FOURTHBUTTON_DOWN,
    POINTER_CHANGE_FOURTHBUTTON_UP,
    POINTER_CHANGE_FIFTHBUTTON_DOWN,
    POINTER_CHANGE_FIFTHBUTTON_UP,
} OWN_POINTER_BUTTON_CHANGE_TYPE;

typedef struct
{
    OWN_POINTER_INPUT_TYPE pointerType;
    UINT32 pointerId;
    UINT32 frameId;
    OWN_POINTER_FLAGS pointerFlags;
    HANDLE sourceDevice;
    HWND hwndTarget;
    POINT ptPixelLocation;
    POINT ptHimetricLocation;
    POINT ptPixelLocationRaw;
    POINT ptHimetricLocationRaw;
    DWORD dwTime;
    UINT32 historyCount;
    INT32 InputData;
    DWORD dwKeyStates;
    UINT64 PerformanceCount;
    OWN_POINTER_BUTTON_CHANGE_TYPE ButtonChangeType;
} POINTER_INFO;

typedef UINT32 OWN_TOUCH_FLAGS;
#define TOUCH_FLAG_NONE 0x00000000 // Default

typedef UINT32 OWN_TOUCH_MASK;
#define OWN_TOUCH_MASK_NONE 0x00000000 // Default - none of the optional fields are valid
#define OWN_TOUCH_MASK_CONTACTAREA 0x00000001 // The rcContact field is valid
#define OWN_TOUCH_MASK_ORIENTATION 0x00000002 // The orientation field is valid
#define OWN_TOUCH_MASK_PRESSURE 0x00000004 // The pressure field is valid

typedef struct
{
    POINTER_INFO pointerInfo;
    OWN_TOUCH_FLAGS touchFlags;
    OWN_TOUCH_MASK touchMask;
    RECT rcContact;
    RECT rcContactRaw;
    UINT32 orientation;
    UINT32 pressure;
} OWN_POINTER_TOUCH_INFO;

#define OWN_TOUCH_FEEDBACK_DEFAULT 0x1
#define OWN_TOUCH_FEEDBACK_INDIRECT 0x2
#define OWN_TOUCH_FEEDBACK_NONE 0x3

typedef BOOL(NTAPI* InitializeTouchInjectionFunction)(UINT32, DWORD);
typedef BOOL(NTAPI* InjectTouchInputFunction)(UINT32, const OWN_POINTER_TOUCH_INFO*);

#endif // HOST_WIN_TOUCH_INJECTOR_DEFINES_H
