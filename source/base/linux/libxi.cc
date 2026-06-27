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

#include "base/linux/libxi.h"

#include <dlfcn.h>

#include "base/logging.h"

namespace {

// Pointer types are taken from the real libXi declarations; only the addresses are resolved dynamically.
void* g_handle = nullptr;
bool g_load_failed = false;

decltype(&XListInputDevices) g_list_input_devices = nullptr;
decltype(&XFreeDeviceList) g_free_device_list = nullptr;
decltype(&XOpenDevice) g_open_device = nullptr;
decltype(&XGetDeviceButtonMapping) g_get_device_button_mapping = nullptr;
decltype(&XSetDeviceButtonMapping) g_set_device_button_mapping = nullptr;
decltype(&XCloseDevice) g_close_device = nullptr;

} // namespace

//--------------------------------------------------------------------------------------------------
// static
bool LibXi::ensureLoaded()
{
    if (g_handle)
        return true;

    // Do not retry: a missing library will not appear later and dlopen is not free.
    if (g_load_failed)
        return false;

    g_handle = dlopen("libXi.so.6", RTLD_LAZY);
    if (!g_handle)
    {
        LOG(ERROR) << "Unable to load libXi.so.6:" << dlerror();
        g_load_failed = true;
        return false;
    }

    g_list_input_devices =
        reinterpret_cast<decltype(g_list_input_devices)>(dlsym(g_handle, "XListInputDevices"));
    g_free_device_list =
        reinterpret_cast<decltype(g_free_device_list)>(dlsym(g_handle, "XFreeDeviceList"));
    g_open_device = reinterpret_cast<decltype(g_open_device)>(dlsym(g_handle, "XOpenDevice"));
    g_get_device_button_mapping = reinterpret_cast<decltype(g_get_device_button_mapping)>(
        dlsym(g_handle, "XGetDeviceButtonMapping"));
    g_set_device_button_mapping = reinterpret_cast<decltype(g_set_device_button_mapping)>(
        dlsym(g_handle, "XSetDeviceButtonMapping"));
    g_close_device = reinterpret_cast<decltype(g_close_device)>(dlsym(g_handle, "XCloseDevice"));

    if (!g_list_input_devices || !g_free_device_list || !g_open_device ||
        !g_get_device_button_mapping || !g_set_device_button_mapping || !g_close_device)
    {
        LOG(ERROR) << "Unable to resolve libXi symbols";
        dlclose(g_handle);
        g_handle = nullptr;
        g_load_failed = true;
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
// static
XDeviceInfo* LibXi::listInputDevices(Display* display, int* ndevices)
{
    if (!ensureLoaded())
        return nullptr;
    return g_list_input_devices(display, ndevices);
}

//--------------------------------------------------------------------------------------------------
// static
void LibXi::freeDeviceList(XDeviceInfo* list)
{
    if (!ensureLoaded())
        return;
    g_free_device_list(list);
}

//--------------------------------------------------------------------------------------------------
// static
XDevice* LibXi::openDevice(Display* display, XID device_id)
{
    if (!ensureLoaded())
        return nullptr;
    return g_open_device(display, device_id);
}

//--------------------------------------------------------------------------------------------------
// static
int LibXi::getDeviceButtonMapping(
    Display* display, XDevice* device, unsigned char* map, unsigned int nmap)
{
    if (!ensureLoaded())
        return 0;
    return g_get_device_button_mapping(display, device, map, nmap);
}

//--------------------------------------------------------------------------------------------------
// static
int LibXi::setDeviceButtonMapping(Display* display, XDevice* device, unsigned char* map, int nmap)
{
    if (!ensureLoaded())
        return 0;
    return g_set_device_button_mapping(display, device, map, nmap);
}

//--------------------------------------------------------------------------------------------------
// static
int LibXi::closeDevice(Display* display, XDevice* device)
{
    if (!ensureLoaded())
        return 0;
    return g_close_device(display, device);
}
