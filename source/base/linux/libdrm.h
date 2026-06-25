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

#ifndef BASE_LINUX_LIBDRM_H
#define BASE_LINUX_LIBDRM_H

#include <QtClassHelperMacros>

#include <xf86drm.h>
#include <xf86drmMode.h>

#include <cstdint>

// Thin wrapper over the subset of libdrm used for KMS scan-out capture. The library is loaded
// dynamically on first use and the resolved symbols are cached, so the binary does not link against
// libdrm. Every method returns a failure result if the library or a symbol is unavailable. The DRM
// device descriptor is owned by the caller and passed in to each call.
class LibDrm
{
    Q_DISABLE_COPY_MOVE(LibDrm)

public:
    // Loads libdrm and resolves its symbols once. Returns false if it is not available.
    static bool ensureLoaded();

    static drmModeRes* modeGetResources(int fd);
    static void modeFreeResources(drmModeRes* resources);
    static drmModeCrtc* modeGetCrtc(int fd, uint32_t crtc_id);
    static void modeFreeCrtc(drmModeCrtc* crtc);
    static drmModeFB2* modeGetFB2(int fd, uint32_t fb_id);
    static void modeFreeFB2(drmModeFB2* fb);
    static int primeHandleToFD(int fd, uint32_t handle, uint32_t flags, int* prime_fd);
    static int closeBufferHandle(int fd, uint32_t handle);
    static int dropMaster(int fd);

    static int setClientCap(int fd, uint64_t capability, uint64_t value);
    static drmModePlaneRes* modeGetPlaneResources(int fd);
    static void modeFreePlaneResources(drmModePlaneRes* resources);
    static drmModePlane* modeGetPlane(int fd, uint32_t plane_id);
    static void modeFreePlane(drmModePlane* plane);

    static drmModeConnector* modeGetConnectorCurrent(int fd, uint32_t connector_id);
    static void modeFreeConnector(drmModeConnector* connector);
    static drmModeEncoder* modeGetEncoder(int fd, uint32_t encoder_id);
    static void modeFreeEncoder(drmModeEncoder* encoder);

    // Maps a dumb buffer for CPU access, returning the mmap offset in |offset|. Used to read small
    // linear buffers (the hardware cursor) without the dmabuf/EGL path, which some virtual GPUs
    // (vmwgfx) cannot export. Returns a negative value if unsupported.
    static int mapDumbBuffer(int fd, uint32_t handle, uint64_t* offset);

    // Object property enumeration, used to read the cursor plane's HOTSPOT_X/HOTSPOT_Y (paravirtual
    // drivers expose the cursor hotspot this way rather than via the plane position).
    static drmModeObjectProperties* modeObjectGetProperties(int fd, uint32_t object_id,
                                                            uint32_t object_type);
    static void modeFreeObjectProperties(drmModeObjectProperties* props);
    static drmModePropertyRes* modeGetProperty(int fd, uint32_t property_id);
    static void modeFreeProperty(drmModePropertyRes* property);
};

#endif // BASE_LINUX_LIBDRM_H
