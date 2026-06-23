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

#ifndef BASE_DESKTOP_LINUX_EGL_DMABUF_H
#define BASE_DESKTOP_LINUX_EGL_DMABUF_H

#include <QList>
#include <QRect>
#include <QSize>

#include <memory>

struct gbm_device;

// Imports PipeWire DMA-BUF frames into CPU memory using EGL/GBM, mirroring the approach used by
// WebRTC. A headless EGL context is created on a DRM render node; each DMA-BUF is turned into an
// EGLImage, bound to a texture and read back as BGRA via a framebuffer. EGL, GL and GBM are loaded
// dynamically (no linking). All resolved symbols and EGL state live in a private object.
class EglDmaBuf
{
public:
    EglDmaBuf();
    ~EglDmaBuf();

    struct Plane
    {
        int fd = -1;
        quint32 offset = 0;
        quint32 stride = 0;
    };

    bool isInitialized() const { return initialized_; }

    // Reads the DMA-BUF described by |planes|/|plane_count|/|modifier| into |dst| (BGRA, |dst_stride|
    // bytes per row). The image is created at |image_size|; only |read_rect| (the valid/cropped
    // region) is read back. Returns false on failure.
    bool imageFromDmaBuf(const QSize& image_size, quint32 fourcc, const Plane* planes,
                         int plane_count, quint64 modifier, const QRect& read_rect,
                         quint8* dst, int dst_stride);

    // Modifiers the driver supports for |fourcc|; used when advertising DMA-BUF formats to the
    // compositor. May be empty (implicit modifier only).
    QList<quint64> queryModifiers(quint32 fourcc);

private:
    struct Egl;

    bool initialize();

    int drm_fd_ = -1;
    gbm_device* gbm_device_ = nullptr;
    std::unique_ptr<Egl> egl_;
    bool initialized_ = false;
};

#endif // BASE_DESKTOP_LINUX_EGL_DMABUF_H
