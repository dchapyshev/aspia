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

#include "host/linux/libdrm.h"

#include <dlfcn.h>

#include <cstring>

#include "base/logging.h"

namespace {

// Pointer types are taken from the real libdrm declarations; only the addresses are resolved
// dynamically.
void* g_handle = nullptr;
bool g_load_failed = false;

decltype(&drmModeGetResources) g_mode_get_resources = nullptr;
decltype(&drmModeFreeResources) g_mode_free_resources = nullptr;
decltype(&drmModeGetCrtc) g_mode_get_crtc = nullptr;
decltype(&drmModeFreeCrtc) g_mode_free_crtc = nullptr;
decltype(&drmModeGetFB2) g_mode_get_fb2 = nullptr;
decltype(&drmModeFreeFB2) g_mode_free_fb2 = nullptr;
decltype(&drmPrimeHandleToFD) g_prime_handle_to_fd = nullptr;
decltype(&drmDropMaster) g_drop_master = nullptr;
decltype(&drmSetClientCap) g_set_client_cap = nullptr;
decltype(&drmModeGetPlaneResources) g_mode_get_plane_resources = nullptr;
decltype(&drmModeFreePlaneResources) g_mode_free_plane_resources = nullptr;
decltype(&drmModeGetPlane) g_mode_get_plane = nullptr;
decltype(&drmModeFreePlane) g_mode_free_plane = nullptr;
decltype(&drmModeGetConnectorCurrent) g_mode_get_connector_current = nullptr;
decltype(&drmModeFreeConnector) g_mode_free_connector = nullptr;
decltype(&drmModeGetEncoder) g_mode_get_encoder = nullptr;
decltype(&drmModeFreeEncoder) g_mode_free_encoder = nullptr;
decltype(&drmIoctl) g_drm_ioctl = nullptr;
decltype(&drmModeObjectGetProperties) g_mode_object_get_properties = nullptr;
decltype(&drmModeFreeObjectProperties) g_mode_free_object_properties = nullptr;
decltype(&drmModeGetProperty) g_mode_get_property = nullptr;
decltype(&drmModeFreeProperty) g_mode_free_property = nullptr;
// drmCloseBufferHandle is newer than some build hosts' libdrm headers, so it is typed by hand and
// resolved at runtime on the target (which ships a recent libdrm).
int (*g_close_buffer_handle)(int fd, uint32_t handle) = nullptr;

} // namespace

//--------------------------------------------------------------------------------------------------
// static
bool LibDrm::ensureLoaded()
{
    if (g_handle)
        return true;

    // Do not retry: a missing library will not appear later and dlopen is not free.
    if (g_load_failed)
        return false;

    g_handle = dlopen("libdrm.so.2", RTLD_LAZY);
    if (!g_handle)
    {
        LOG(ERROR) << "Unable to load libdrm.so.2:" << dlerror();
        g_load_failed = true;
        return false;
    }

    g_mode_get_resources =
        reinterpret_cast<decltype(g_mode_get_resources)>(dlsym(g_handle, "drmModeGetResources"));
    g_mode_free_resources =
        reinterpret_cast<decltype(g_mode_free_resources)>(dlsym(g_handle, "drmModeFreeResources"));
    g_mode_get_crtc =
        reinterpret_cast<decltype(g_mode_get_crtc)>(dlsym(g_handle, "drmModeGetCrtc"));
    g_mode_free_crtc =
        reinterpret_cast<decltype(g_mode_free_crtc)>(dlsym(g_handle, "drmModeFreeCrtc"));
    g_mode_get_fb2 =
        reinterpret_cast<decltype(g_mode_get_fb2)>(dlsym(g_handle, "drmModeGetFB2"));
    g_mode_free_fb2 =
        reinterpret_cast<decltype(g_mode_free_fb2)>(dlsym(g_handle, "drmModeFreeFB2"));
    g_prime_handle_to_fd =
        reinterpret_cast<decltype(g_prime_handle_to_fd)>(dlsym(g_handle, "drmPrimeHandleToFD"));
    g_drop_master =
        reinterpret_cast<decltype(g_drop_master)>(dlsym(g_handle, "drmDropMaster"));
    g_close_buffer_handle =
        reinterpret_cast<decltype(g_close_buffer_handle)>(dlsym(g_handle, "drmCloseBufferHandle"));
    g_set_client_cap =
        reinterpret_cast<decltype(g_set_client_cap)>(dlsym(g_handle, "drmSetClientCap"));
    g_mode_get_plane_resources =
        reinterpret_cast<decltype(g_mode_get_plane_resources)>(dlsym(g_handle, "drmModeGetPlaneResources"));
    g_mode_free_plane_resources =
        reinterpret_cast<decltype(g_mode_free_plane_resources)>(dlsym(g_handle, "drmModeFreePlaneResources"));
    g_mode_get_plane =
        reinterpret_cast<decltype(g_mode_get_plane)>(dlsym(g_handle, "drmModeGetPlane"));
    g_mode_free_plane =
        reinterpret_cast<decltype(g_mode_free_plane)>(dlsym(g_handle, "drmModeFreePlane"));
    g_mode_get_connector_current =
        reinterpret_cast<decltype(g_mode_get_connector_current)>(dlsym(g_handle, "drmModeGetConnectorCurrent"));
    g_mode_free_connector =
        reinterpret_cast<decltype(g_mode_free_connector)>(dlsym(g_handle, "drmModeFreeConnector"));
    g_mode_get_encoder =
        reinterpret_cast<decltype(g_mode_get_encoder)>(dlsym(g_handle, "drmModeGetEncoder"));
    g_mode_free_encoder =
        reinterpret_cast<decltype(g_mode_free_encoder)>(dlsym(g_handle, "drmModeFreeEncoder"));
    // Optional (resolved but not required): used only for the cursor CPU read path.
    g_drm_ioctl = reinterpret_cast<decltype(g_drm_ioctl)>(dlsym(g_handle, "drmIoctl"));
    g_mode_object_get_properties = reinterpret_cast<decltype(g_mode_object_get_properties)>(
        dlsym(g_handle, "drmModeObjectGetProperties"));
    g_mode_free_object_properties = reinterpret_cast<decltype(g_mode_free_object_properties)>(
        dlsym(g_handle, "drmModeFreeObjectProperties"));
    g_mode_get_property =
        reinterpret_cast<decltype(g_mode_get_property)>(dlsym(g_handle, "drmModeGetProperty"));
    g_mode_free_property =
        reinterpret_cast<decltype(g_mode_free_property)>(dlsym(g_handle, "drmModeFreeProperty"));

    if (!g_mode_get_resources || !g_mode_free_resources || !g_mode_get_crtc || !g_mode_free_crtc ||
        !g_mode_get_fb2 || !g_mode_free_fb2 || !g_prime_handle_to_fd || !g_drop_master ||
        !g_close_buffer_handle || !g_set_client_cap || !g_mode_get_plane_resources ||
        !g_mode_free_plane_resources || !g_mode_get_plane || !g_mode_free_plane ||
        !g_mode_get_connector_current || !g_mode_free_connector || !g_mode_get_encoder ||
        !g_mode_free_encoder)
    {
        LOG(ERROR) << "Unable to resolve libdrm symbols";
        dlclose(g_handle);
        g_handle = nullptr;
        g_load_failed = true;
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
// static
drmModeRes* LibDrm::modeGetResources(int fd)
{
    if (!ensureLoaded())
        return nullptr;
    return g_mode_get_resources(fd);
}

//--------------------------------------------------------------------------------------------------
// static
void LibDrm::modeFreeResources(drmModeRes* resources)
{
    if (!ensureLoaded())
        return;
    g_mode_free_resources(resources);
}

//--------------------------------------------------------------------------------------------------
// static
drmModeCrtc* LibDrm::modeGetCrtc(int fd, uint32_t crtc_id)
{
    if (!ensureLoaded())
        return nullptr;
    return g_mode_get_crtc(fd, crtc_id);
}

//--------------------------------------------------------------------------------------------------
// static
void LibDrm::modeFreeCrtc(drmModeCrtc* crtc)
{
    if (!ensureLoaded())
        return;
    g_mode_free_crtc(crtc);
}

//--------------------------------------------------------------------------------------------------
// static
drmModeFB2* LibDrm::modeGetFB2(int fd, uint32_t fb_id)
{
    if (!ensureLoaded())
        return nullptr;
    return g_mode_get_fb2(fd, fb_id);
}

//--------------------------------------------------------------------------------------------------
// static
void LibDrm::modeFreeFB2(drmModeFB2* fb)
{
    if (!ensureLoaded())
        return;
    g_mode_free_fb2(fb);
}

//--------------------------------------------------------------------------------------------------
// static
int LibDrm::primeHandleToFD(int fd, uint32_t handle, uint32_t flags, int* prime_fd)
{
    if (!ensureLoaded())
        return -1;
    return g_prime_handle_to_fd(fd, handle, flags, prime_fd);
}

//--------------------------------------------------------------------------------------------------
// static
int LibDrm::closeBufferHandle(int fd, uint32_t handle)
{
    if (!ensureLoaded())
        return -1;
    return g_close_buffer_handle(fd, handle);
}

//--------------------------------------------------------------------------------------------------
// static
int LibDrm::dropMaster(int fd)
{
    if (!ensureLoaded())
        return -1;
    return g_drop_master(fd);
}

//--------------------------------------------------------------------------------------------------
// static
int LibDrm::setClientCap(int fd, uint64_t capability, uint64_t value)
{
    if (!ensureLoaded())
        return -1;
    return g_set_client_cap(fd, capability, value);
}

//--------------------------------------------------------------------------------------------------
// static
drmModePlaneRes* LibDrm::modeGetPlaneResources(int fd)
{
    if (!ensureLoaded())
        return nullptr;
    return g_mode_get_plane_resources(fd);
}

//--------------------------------------------------------------------------------------------------
// static
void LibDrm::modeFreePlaneResources(drmModePlaneRes* resources)
{
    if (!ensureLoaded())
        return;
    g_mode_free_plane_resources(resources);
}

//--------------------------------------------------------------------------------------------------
// static
drmModePlane* LibDrm::modeGetPlane(int fd, uint32_t plane_id)
{
    if (!ensureLoaded())
        return nullptr;
    return g_mode_get_plane(fd, plane_id);
}

//--------------------------------------------------------------------------------------------------
// static
void LibDrm::modeFreePlane(drmModePlane* plane)
{
    if (!ensureLoaded())
        return;
    g_mode_free_plane(plane);
}

//--------------------------------------------------------------------------------------------------
// static
drmModeConnector* LibDrm::modeGetConnectorCurrent(int fd, uint32_t connector_id)
{
    if (!ensureLoaded())
        return nullptr;
    return g_mode_get_connector_current(fd, connector_id);
}

//--------------------------------------------------------------------------------------------------
// static
void LibDrm::modeFreeConnector(drmModeConnector* connector)
{
    if (!ensureLoaded())
        return;
    g_mode_free_connector(connector);
}

//--------------------------------------------------------------------------------------------------
// static
drmModeEncoder* LibDrm::modeGetEncoder(int fd, uint32_t encoder_id)
{
    if (!ensureLoaded())
        return nullptr;
    return g_mode_get_encoder(fd, encoder_id);
}

//--------------------------------------------------------------------------------------------------
// static
void LibDrm::modeFreeEncoder(drmModeEncoder* encoder)
{
    if (!ensureLoaded())
        return;
    g_mode_free_encoder(encoder);
}

//--------------------------------------------------------------------------------------------------
// static
int LibDrm::mapDumbBuffer(int fd, uint32_t handle, uint64_t* offset)
{
    if (!ensureLoaded() || !g_drm_ioctl)
        return -1;

    struct drm_mode_map_dumb arg;
    memset(&arg, 0, sizeof(arg));
    arg.handle = handle;

    const int result = g_drm_ioctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &arg);
    if (result == 0 && offset)
        *offset = arg.offset;
    return result;
}

//--------------------------------------------------------------------------------------------------
// static
drmModeObjectProperties* LibDrm::modeObjectGetProperties(int fd, uint32_t object_id,
                                                         uint32_t object_type)
{
    if (!ensureLoaded() || !g_mode_object_get_properties)
        return nullptr;
    return g_mode_object_get_properties(fd, object_id, object_type);
}

//--------------------------------------------------------------------------------------------------
// static
void LibDrm::modeFreeObjectProperties(drmModeObjectProperties* props)
{
    if (!ensureLoaded() || !g_mode_free_object_properties)
        return;
    g_mode_free_object_properties(props);
}

//--------------------------------------------------------------------------------------------------
// static
drmModePropertyRes* LibDrm::modeGetProperty(int fd, uint32_t property_id)
{
    if (!ensureLoaded() || !g_mode_get_property)
        return nullptr;
    return g_mode_get_property(fd, property_id);
}

//--------------------------------------------------------------------------------------------------
// static
void LibDrm::modeFreeProperty(drmModePropertyRes* property)
{
    if (!ensureLoaded() || !g_mode_free_property)
        return;
    g_mode_free_property(property);
}
