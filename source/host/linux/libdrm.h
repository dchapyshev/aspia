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

#ifndef HOST_LINUX_LIBDRM_H
#define HOST_LINUX_LIBDRM_H

#include <QtClassHelperMacros>

#include <xf86drm.h>
#include <xf86drmMode.h>

#include <cstdint>
#include <memory>

// Thin wrapper over the subset of libdrm used for KMS scan-out capture. The library is loaded
// dynamically on first use and the resolved symbols are cached, so the binary does not link against
// libdrm. Every method returns a failure result if the library or a symbol is unavailable. The DRM
// device descriptor is owned by the caller and passed in to each call.
class LibDrm
{
    Q_DISABLE_COPY_MOVE(LibDrm)

public:
    struct ResourcesDeleter { void operator()(drmModeRes* resources) const { modeFreeResources(resources); } };
    using ScopedResources = std::unique_ptr<drmModeRes, ResourcesDeleter>;

    struct CrtcDeleter { void operator()(drmModeCrtc* crtc) const { modeFreeCrtc(crtc); } };
    using ScopedCrtc = std::unique_ptr<drmModeCrtc, CrtcDeleter>;

    struct FB2Deleter { void operator()(drmModeFB2* fb) const { modeFreeFB2(fb); } };
    using ScopedFB2 = std::unique_ptr<drmModeFB2, FB2Deleter>;

    struct PlaneResourcesDeleter { void operator()(drmModePlaneRes* res) const { modeFreePlaneResources(res); } };
    using ScopedPlaneResources = std::unique_ptr<drmModePlaneRes, PlaneResourcesDeleter>;

    struct PlaneDeleter { void operator()(drmModePlane* plane) const { modeFreePlane(plane); } };
    using ScopedPlane = std::unique_ptr<drmModePlane, PlaneDeleter>;

    struct ConnectorDeleter { void operator()(drmModeConnector* connector) const { modeFreeConnector(connector); } };
    using ScopedConnector = std::unique_ptr<drmModeConnector, ConnectorDeleter>;

    struct EncoderDeleter { void operator()(drmModeEncoder* encoder) const { modeFreeEncoder(encoder); } };
    using ScopedEncoder = std::unique_ptr<drmModeEncoder, EncoderDeleter>;

    struct ObjectPropertiesDeleter
    {
        void operator()(drmModeObjectProperties* props) const { modeFreeObjectProperties(props); }
    };
    using ScopedObjectProperties = std::unique_ptr<drmModeObjectProperties, ObjectPropertiesDeleter>;

    struct PropertyDeleter { void operator()(drmModePropertyRes* property) const { modeFreeProperty(property); } };
    using ScopedProperty = std::unique_ptr<drmModePropertyRes, PropertyDeleter>;

    // Loads libdrm and resolves its symbols once. Returns false if it is not available.
    static bool ensureLoaded();

    static ScopedResources modeGetResources(int fd);
    static ScopedCrtc modeGetCrtc(int fd, uint32_t crtc_id);
    static ScopedFB2 modeGetFB2(int fd, uint32_t fb_id);
    static int primeHandleToFD(int fd, uint32_t handle, uint32_t flags, int* prime_fd);
    static int closeBufferHandle(int fd, uint32_t handle);
    static int dropMaster(int fd);

    static int setClientCap(int fd, uint64_t capability, uint64_t value);
    static ScopedPlaneResources modeGetPlaneResources(int fd);
    static ScopedPlane modeGetPlane(int fd, uint32_t plane_id);

    static ScopedConnector modeGetConnectorCurrent(int fd, uint32_t connector_id);
    static ScopedEncoder modeGetEncoder(int fd, uint32_t encoder_id);

    // Maps a dumb buffer for CPU access, returning the mmap offset in |offset|. Used to read small
    // linear buffers (the hardware cursor) without the dmabuf/EGL path, which some virtual GPUs
    // (vmwgfx) cannot export. Returns a negative value if unsupported.
    static int mapDumbBuffer(int fd, uint32_t handle, uint64_t* offset);

    // Object property enumeration, used to read the cursor plane's HOTSPOT_X/HOTSPOT_Y (paravirtual
    // drivers expose the cursor hotspot this way rather than via the plane position).
    static ScopedObjectProperties modeObjectGetProperties(int fd, uint32_t object_id, uint32_t object_type);
    static ScopedProperty modeGetProperty(int fd, uint32_t property_id);

private:
    static void modeFreeResources(drmModeRes* resources);
    static void modeFreeCrtc(drmModeCrtc* crtc);
    static void modeFreeFB2(drmModeFB2* fb);
    static void modeFreePlaneResources(drmModePlaneRes* resources);
    static void modeFreePlane(drmModePlane* plane);
    static void modeFreeConnector(drmModeConnector* connector);
    static void modeFreeEncoder(drmModeEncoder* encoder);
    static void modeFreeObjectProperties(drmModeObjectProperties* props);
    static void modeFreeProperty(drmModePropertyRes* property);
};

#endif // HOST_LINUX_LIBDRM_H
