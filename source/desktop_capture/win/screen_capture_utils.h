//
// PROJECT:         Aspia
// FILE:            desktop_capture/win/screen_capture_utils.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
// NOTE:            This file based on WebRTC source code
//

#ifndef ASPIA_DESKTOP_CAPTURE__WIN__SCREEN_CAPTURE_UTILS_H_
#define ASPIA_DESKTOP_CAPTURE__WIN__SCREEN_CAPTURE_UTILS_H_

#include <QRect>
#include <QString>
#include <QVector>

#include "desktop_capture/screen_capturer.h"

namespace aspia {

// Output the list of active screens into |screens|. Returns true if succeeded, or false if it
// fails to enumerate the display devices. If the |device_names| is provided, it will be filled
// with the DISPLAY_DEVICE.DeviceName in UTF-8 encoding. If this function returns true, consumers
// can always assume that |screens|[i] and |device_names|[i] indicate the same monitor on the
// system.
bool screenList(ScreenCapturer::ScreenList* screens, QVector<QString>* device_names = nullptr);

// Returns true if |screen| is a valid screen. The screen device key is returned through
// |device_key| if the screen is valid. The device key can be used in GetScreenRect to verify the
// screen matches the previously obtained id.
bool isScreenValid(ScreenCapturer::ScreenId screen, QString* device_key);

// Get the rect of the entire system in system coordinate system. I.e. the primary monitor always
// starts from (0, 0).
QRect fullScreenRect();

// Get the rect of the screen identified by |screen|, relative to the primary display's top-left.
// If the screen device key does not match |device_key|, or the screen does not exist, or any error
// happens, an empty rect is returned.
QRect screenRect(ScreenCapturer::ScreenId screen, const QString& device_key);

} // namespace aspia

#endif // ASPIA_DESKTOP_CAPTURE__WIN__SCREEN_CAPTURE_UTILS_H_
