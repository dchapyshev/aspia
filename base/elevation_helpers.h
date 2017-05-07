//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/elevation_helpers.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__ELEVATION_HELPERS_H
#define _ASPIA_BASE__ELEVATION_HELPERS_H

namespace aspia {

bool ElevateProcess();
bool IsProcessElevated();

} // namespace aspia

#endif // _ASPIA_BASE__ELEVATION_HELPERS_H
