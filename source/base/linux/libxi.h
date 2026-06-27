//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef BASE_LINUX_LIBXI_H
#define BASE_LINUX_LIBXI_H

#include <QtClassHelperMacros>

#include "base/linux/x11_headers.h"

// Thin wrapper over the subset of libXi (the XInput extension) used to query and normalize the pointer
// button mapping. The library is loaded dynamically on first use and the resolved symbols are cached, so
// the binary does not link against libXi. Every call is a no-op (returning a zero result) if the library
// or a symbol is unavailable.
class LibXi
{
    Q_DISABLE_COPY_MOVE(LibXi)

public:
    // Loads libXi and resolves its symbols once. Returns false if it is not available.
    static bool ensureLoaded();

    static XDeviceInfo* listInputDevices(Display* display, int* ndevices);
    static void freeDeviceList(XDeviceInfo* list);
    static XDevice* openDevice(Display* display, XID device_id);
    static int getDeviceButtonMapping(
        Display* display, XDevice* device, unsigned char* map, unsigned int nmap);
    static int setDeviceButtonMapping(Display* display, XDevice* device, unsigned char* map, int nmap);
    static int closeDevice(Display* display, XDevice* device);
};

#endif // BASE_LINUX_LIBXI_H
