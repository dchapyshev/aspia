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

#include "base/desktop/linux/egl_dmabuf.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <gbm.h>
#include <drm/drm_fourcc.h>
#include <xf86drm.h>

#include <dlfcn.h>
#include <fcntl.h>
#include <unistd.h>

#include <array>
#include <cstring>

#include "base/logging.h"

namespace {

// GL types and constants. They are declared here (rather than pulling in <GL/gl.h> and the various
// extension headers, which conflict) because every GL entry point is resolved dynamically.
using GLenum = unsigned int;
using GLuint = unsigned int;
using GLint = int;
using GLsizei = int;

const GLenum GL_TEXTURE_2D_GL = 0x0DE1;
const GLenum GL_TEXTURE_MIN_FILTER_GL = 0x2801;
const GLenum GL_TEXTURE_MAG_FILTER_GL = 0x2800;
const GLint GL_NEAREST_GL = 0x2600;
const GLenum GL_FRAMEBUFFER_GL = 0x8D40;
const GLenum GL_COLOR_ATTACHMENT0_GL = 0x8CE0;
const GLenum GL_FRAMEBUFFER_COMPLETE_GL = 0x8CD5;
const GLenum GL_BGRA_GL = 0x80E1;
const GLenum GL_UNSIGNED_BYTE_GL = 0x1401;
const GLenum GL_PACK_ALIGNMENT_GL = 0x0D05;
const GLenum GL_PACK_ROW_LENGTH_GL = 0x0D02;
const GLenum GL_NO_ERROR_GL = 0;

using glGenTextures_t = void (*)(GLsizei, GLuint*);
using glBindTexture_t = void (*)(GLenum, GLuint);
using glDeleteTextures_t = void (*)(GLsizei, const GLuint*);
using glTexParameteri_t = void (*)(GLenum, GLenum, GLint);
using glGenFramebuffers_t = void (*)(GLsizei, GLuint*);
using glBindFramebuffer_t = void (*)(GLenum, GLuint);
using glDeleteFramebuffers_t = void (*)(GLsizei, const GLuint*);
using glFramebufferTexture2D_t = void (*)(GLenum, GLenum, GLenum, GLuint, GLint);
using glCheckFramebufferStatus_t = GLenum (*)(GLenum);
using glReadPixels_t = void (*)(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void*);
using glPixelStorei_t = void (*)(GLenum, GLint);
using glGetError_t = GLenum (*)();
using glEglImageTargetTexture2D_t = void (*)(GLenum, void*);

const int kMaxPlanes = 4;

// Plane attribute enums, indexed by plane number.
const EGLint kPlaneFd[kMaxPlanes] = {
    EGL_DMA_BUF_PLANE0_FD_EXT, EGL_DMA_BUF_PLANE1_FD_EXT,
    EGL_DMA_BUF_PLANE2_FD_EXT, EGL_DMA_BUF_PLANE3_FD_EXT
};
const EGLint kPlaneOffset[kMaxPlanes] = {
    EGL_DMA_BUF_PLANE0_OFFSET_EXT, EGL_DMA_BUF_PLANE1_OFFSET_EXT,
    EGL_DMA_BUF_PLANE2_OFFSET_EXT, EGL_DMA_BUF_PLANE3_OFFSET_EXT
};
const EGLint kPlanePitch[kMaxPlanes] = {
    EGL_DMA_BUF_PLANE0_PITCH_EXT, EGL_DMA_BUF_PLANE1_PITCH_EXT,
    EGL_DMA_BUF_PLANE2_PITCH_EXT, EGL_DMA_BUF_PLANE3_PITCH_EXT
};
const EGLint kPlaneModifierLo[kMaxPlanes] = {
    EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT, EGL_DMA_BUF_PLANE1_MODIFIER_LO_EXT,
    EGL_DMA_BUF_PLANE2_MODIFIER_LO_EXT, EGL_DMA_BUF_PLANE3_MODIFIER_LO_EXT
};
const EGLint kPlaneModifierHi[kMaxPlanes] = {
    EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT, EGL_DMA_BUF_PLANE1_MODIFIER_HI_EXT,
    EGL_DMA_BUF_PLANE2_MODIFIER_HI_EXT, EGL_DMA_BUF_PLANE3_MODIFIER_HI_EXT
};

} // namespace

// Resolved EGL/GL symbols and the EGL state. Core EGL members are prefixed |fp_| because the plain
// names collide with the prototypes declared in <EGL/egl.h>.
struct EglDmaBuf::Egl
{
    void* egl_handle = nullptr;
    void* gbm_handle = nullptr;

    EGLDisplay display = EGL_NO_DISPLAY;
    EGLContext context = EGL_NO_CONTEXT;

    decltype(&::eglGetProcAddress) fp_eglGetProcAddress = nullptr;
    decltype(&::eglGetError) fp_eglGetError = nullptr;
    decltype(&::eglInitialize) fp_eglInitialize = nullptr;
    decltype(&::eglTerminate) fp_eglTerminate = nullptr;
    decltype(&::eglBindAPI) fp_eglBindAPI = nullptr;
    decltype(&::eglCreateContext) fp_eglCreateContext = nullptr;
    decltype(&::eglDestroyContext) fp_eglDestroyContext = nullptr;
    decltype(&::eglMakeCurrent) fp_eglMakeCurrent = nullptr;
    decltype(&::eglQueryString) fp_eglQueryString = nullptr;
    decltype(&::eglChooseConfig) fp_eglChooseConfig = nullptr;

    bool modifiers_supported = false;

    PFNEGLGETPLATFORMDISPLAYEXTPROC eglGetPlatformDisplayEXT = nullptr;
    PFNEGLCREATEIMAGEKHRPROC eglCreateImageKHR = nullptr;
    PFNEGLDESTROYIMAGEKHRPROC eglDestroyImageKHR = nullptr;
    PFNEGLQUERYDMABUFMODIFIERSEXTPROC eglQueryDmaBufModifiersEXT = nullptr;

    glGenTextures_t glGenTextures = nullptr;
    glBindTexture_t glBindTexture = nullptr;
    glDeleteTextures_t glDeleteTextures = nullptr;
    glTexParameteri_t glTexParameteri = nullptr;
    glGenFramebuffers_t glGenFramebuffers = nullptr;
    glBindFramebuffer_t glBindFramebuffer = nullptr;
    glDeleteFramebuffers_t glDeleteFramebuffers = nullptr;
    glFramebufferTexture2D_t glFramebufferTexture2D = nullptr;
    glCheckFramebufferStatus_t glCheckFramebufferStatus = nullptr;
    glReadPixels_t glReadPixels = nullptr;
    glPixelStorei_t glPixelStorei = nullptr;
    glGetError_t glGetError = nullptr;
    glEglImageTargetTexture2D_t glEGLImageTargetTexture2DOES = nullptr;

    decltype(&::gbm_device_destroy) gbm_device_destroy_fn = nullptr;
};

//--------------------------------------------------------------------------------------------------
EglDmaBuf::EglDmaBuf()
    : egl_(std::make_unique<Egl>())
{
    initialized_ = initialize();
    if (!initialized_)
        LOG(WARNING) << "EGL DMA-BUF import is not available";
}

//--------------------------------------------------------------------------------------------------
EglDmaBuf::~EglDmaBuf()
{
    if (egl_->display != EGL_NO_DISPLAY)
    {
        if (egl_->fp_eglMakeCurrent)
            egl_->fp_eglMakeCurrent(egl_->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (egl_->context != EGL_NO_CONTEXT && egl_->fp_eglDestroyContext)
            egl_->fp_eglDestroyContext(egl_->display, egl_->context);
        if (egl_->fp_eglTerminate)
            egl_->fp_eglTerminate(egl_->display);
    }

    if (gbm_device_ && egl_->gbm_device_destroy_fn)
        egl_->gbm_device_destroy_fn(gbm_device_);
    if (drm_fd_ >= 0)
        ::close(drm_fd_);

    if (egl_->gbm_handle)
        dlclose(egl_->gbm_handle);
    if (egl_->egl_handle)
        dlclose(egl_->egl_handle);
}

//--------------------------------------------------------------------------------------------------
bool EglDmaBuf::imageFromDmaBuf(const QSize& image_size, quint32 fourcc, const Plane* planes,
                                int plane_count, quint64 modifier, const QRect& read_rect,
                                quint8* dst, int dst_stride)
{
    if (!initialized_ || plane_count < 1 || plane_count > kMaxPlanes)
        return false;

    if (!egl_->fp_eglMakeCurrent(egl_->display, EGL_NO_SURFACE, EGL_NO_SURFACE, egl_->context))
        return false;

    // Try importing with the modifier first; if that fails, retry without it (implicit modifier),
    // matching WebRTC's fallback.
    EGLImageKHR image = EGL_NO_IMAGE_KHR;
    for (int attempt = 0; attempt < 2 && image == EGL_NO_IMAGE_KHR; ++attempt)
    {
        const bool use_modifier =
            (modifier != DRM_FORMAT_MOD_INVALID) && egl_->modifiers_supported && (attempt == 0);

        // eglCreateImageKHR takes 32-bit EGLint attributes (the modifier is split into two halves).
        std::array<EGLint, 64> attribs;
        int n = 0;
        attribs[n++] = EGL_WIDTH;                attribs[n++] = image_size.width();
        attribs[n++] = EGL_HEIGHT;               attribs[n++] = image_size.height();
        attribs[n++] = EGL_LINUX_DRM_FOURCC_EXT; attribs[n++] = static_cast<EGLint>(fourcc);

        for (int i = 0; i < plane_count; ++i)
        {
            attribs[n++] = kPlaneFd[i];     attribs[n++] = planes[i].fd;
            attribs[n++] = kPlaneOffset[i]; attribs[n++] = static_cast<EGLint>(planes[i].offset);
            attribs[n++] = kPlanePitch[i];  attribs[n++] = static_cast<EGLint>(planes[i].stride);

            if (use_modifier)
            {
                attribs[n++] = kPlaneModifierLo[i];
                attribs[n++] = static_cast<EGLint>(modifier & 0xffffffff);
                attribs[n++] = kPlaneModifierHi[i];
                attribs[n++] = static_cast<EGLint>(modifier >> 32);
            }
        }
        attribs[n++] = EGL_NONE;

        image = egl_->eglCreateImageKHR(
            egl_->display, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, nullptr, attribs.data());

        // Without a modifier there is nothing left to retry.
        if (!use_modifier)
            break;
    }

    if (image == EGL_NO_IMAGE_KHR)
    {
        LOG(ERROR) << "eglCreateImageKHR failed:" << egl_->fp_eglGetError();
        return false;
    }

    GLuint texture = 0;
    egl_->glGenTextures(1, &texture);
    egl_->glBindTexture(GL_TEXTURE_2D_GL, texture);
    egl_->glTexParameteri(GL_TEXTURE_2D_GL, GL_TEXTURE_MIN_FILTER_GL, GL_NEAREST_GL);
    egl_->glTexParameteri(GL_TEXTURE_2D_GL, GL_TEXTURE_MAG_FILTER_GL, GL_NEAREST_GL);
    egl_->glEGLImageTargetTexture2DOES(GL_TEXTURE_2D_GL, image);

    GLuint framebuffer = 0;
    egl_->glGenFramebuffers(1, &framebuffer);
    egl_->glBindFramebuffer(GL_FRAMEBUFFER_GL, framebuffer);
    egl_->glFramebufferTexture2D(
        GL_FRAMEBUFFER_GL, GL_COLOR_ATTACHMENT0_GL, GL_TEXTURE_2D_GL, texture, 0);

    bool result = false;
    if (egl_->glCheckFramebufferStatus(GL_FRAMEBUFFER_GL) == GL_FRAMEBUFFER_COMPLETE_GL)
    {
        egl_->glPixelStorei(GL_PACK_ALIGNMENT_GL, 1);
        egl_->glPixelStorei(GL_PACK_ROW_LENGTH_GL, dst_stride / 4);
        egl_->glReadPixels(read_rect.x(), read_rect.y(), read_rect.width(), read_rect.height(),
                           GL_BGRA_GL, GL_UNSIGNED_BYTE_GL, dst);
        egl_->glPixelStorei(GL_PACK_ROW_LENGTH_GL, 0);

        // Report failure (so the caller can fall back) if the readback did not succeed.
        result = (egl_->glGetError() == GL_NO_ERROR_GL);
    }
    else
    {
        LOG(ERROR) << "Framebuffer is not complete";
    }

    egl_->glBindFramebuffer(GL_FRAMEBUFFER_GL, 0);
    egl_->glDeleteFramebuffers(1, &framebuffer);
    egl_->glDeleteTextures(1, &texture);
    egl_->eglDestroyImageKHR(egl_->display, image);

    return result;
}

//--------------------------------------------------------------------------------------------------
QList<quint64> EglDmaBuf::queryModifiers(quint32 fourcc)
{
    QList<quint64> result;
    if (!initialized_ || !egl_->modifiers_supported)
        return result;

    EGLint count = 0;
    if (!egl_->eglQueryDmaBufModifiersEXT(
            egl_->display, static_cast<EGLint>(fourcc), 0, nullptr, nullptr, &count) || count <= 0)
    {
        return result;
    }

    QList<EGLuint64KHR> modifiers;
    modifiers.resize(count);
    if (egl_->eglQueryDmaBufModifiersEXT(
            egl_->display, static_cast<EGLint>(fourcc), count, modifiers.data(), nullptr, &count))
    {
        for (EGLuint64KHR modifier : std::as_const(modifiers))
            result.append(static_cast<quint64>(modifier));
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
bool EglDmaBuf::initialize()
{
    egl_->egl_handle = dlopen("libEGL.so.1", RTLD_LAZY);
    if (!egl_->egl_handle)
    {
        LOG(WARNING) << "Unable to load libEGL.so.1:" << dlerror();
        return false;
    }

    egl_->gbm_handle = dlopen("libgbm.so.1", RTLD_LAZY);
    if (!egl_->gbm_handle)
    {
        LOG(WARNING) << "Unable to load libgbm.so.1:" << dlerror();
        return false;
    }

    egl_->fp_eglGetProcAddress = reinterpret_cast<decltype(&::eglGetProcAddress)>(
        dlsym(egl_->egl_handle, "eglGetProcAddress"));
    if (!egl_->fp_eglGetProcAddress)
        return false;

#define RESOLVE_EGL(fn)                                                                       \
    egl_->fp_##fn = reinterpret_cast<decltype(&::fn)>(dlsym(egl_->egl_handle, #fn));           \
    if (!egl_->fp_##fn) { LOG(WARNING) << "Missing EGL symbol:" << #fn; return false; }

    RESOLVE_EGL(eglGetError)
    RESOLVE_EGL(eglInitialize)
    RESOLVE_EGL(eglTerminate)
    RESOLVE_EGL(eglBindAPI)
    RESOLVE_EGL(eglCreateContext)
    RESOLVE_EGL(eglDestroyContext)
    RESOLVE_EGL(eglMakeCurrent)
    RESOLVE_EGL(eglQueryString)
    RESOLVE_EGL(eglChooseConfig)
#undef RESOLVE_EGL

#define RESOLVE_PROC(sym)                                                                     \
    egl_->sym = reinterpret_cast<decltype(egl_->sym)>(egl_->fp_eglGetProcAddress(#sym));       \
    if (!egl_->sym) { LOG(WARNING) << "Missing symbol:" << #sym; return false; }

    RESOLVE_PROC(eglGetPlatformDisplayEXT)
    RESOLVE_PROC(eglCreateImageKHR)
    RESOLVE_PROC(eglDestroyImageKHR)
    RESOLVE_PROC(eglQueryDmaBufModifiersEXT)
    RESOLVE_PROC(glGenTextures)
    RESOLVE_PROC(glBindTexture)
    RESOLVE_PROC(glDeleteTextures)
    RESOLVE_PROC(glTexParameteri)
    RESOLVE_PROC(glGenFramebuffers)
    RESOLVE_PROC(glBindFramebuffer)
    RESOLVE_PROC(glDeleteFramebuffers)
    RESOLVE_PROC(glFramebufferTexture2D)
    RESOLVE_PROC(glCheckFramebufferStatus)
    RESOLVE_PROC(glReadPixels)
    RESOLVE_PROC(glPixelStorei)
    RESOLVE_PROC(glGetError)
    RESOLVE_PROC(glEGLImageTargetTexture2DOES)
#undef RESOLVE_PROC

    auto gbm_create_device_fn = reinterpret_cast<decltype(&gbm_create_device)>(
        dlsym(egl_->gbm_handle, "gbm_create_device"));
    egl_->gbm_device_destroy_fn = reinterpret_cast<decltype(&::gbm_device_destroy)>(
        dlsym(egl_->gbm_handle, "gbm_device_destroy"));
    if (!gbm_create_device_fn || !egl_->gbm_device_destroy_fn)
        return false;

    // Open a DRM render node for the off-screen GBM/EGL device. Enumerate them with libdrm; fall
    // back to the conventional paths if libdrm is unavailable.
    void* drm_handle = dlopen("libdrm.so.2", RTLD_LAZY);
    if (drm_handle)
    {
        auto get_devices = reinterpret_cast<decltype(&drmGetDevices2)>(
            dlsym(drm_handle, "drmGetDevices2"));
        auto free_devices = reinterpret_cast<decltype(&drmFreeDevices)>(
            dlsym(drm_handle, "drmFreeDevices"));

        if (get_devices && free_devices)
        {
            drmDevicePtr devices[16];
            const int count = get_devices(0, devices, 16);
            for (int i = 0; i < count && drm_fd_ < 0; ++i)
            {
                if (devices[i]->available_nodes & (1 << DRM_NODE_RENDER))
                    drm_fd_ = ::open(devices[i]->nodes[DRM_NODE_RENDER], O_RDWR | O_CLOEXEC);
            }
            if (count > 0)
                free_devices(devices, count);
        }

        dlclose(drm_handle);
    }

    if (drm_fd_ < 0)
    {
        static const char* const kRenderNodes[] = {
            "/dev/dri/renderD128", "/dev/dri/renderD129", "/dev/dri/card0"
        };
        for (const char* node : kRenderNodes)
        {
            drm_fd_ = ::open(node, O_RDWR | O_CLOEXEC);
            if (drm_fd_ >= 0)
                break;
        }
    }

    if (drm_fd_ < 0)
    {
        LOG(WARNING) << "Unable to open a DRM render node";
        return false;
    }

    gbm_device_ = gbm_create_device_fn(drm_fd_);
    if (!gbm_device_)
    {
        LOG(WARNING) << "gbm_create_device failed";
        return false;
    }

    egl_->display = egl_->eglGetPlatformDisplayEXT(EGL_PLATFORM_GBM_KHR, gbm_device_, nullptr);
    if (egl_->display == EGL_NO_DISPLAY)
    {
        LOG(WARNING) << "eglGetPlatformDisplayEXT failed";
        return false;
    }

    EGLint major = 0;
    EGLint minor = 0;
    if (!egl_->fp_eglInitialize(egl_->display, &major, &minor))
    {
        LOG(WARNING) << "eglInitialize failed:" << egl_->fp_eglGetError();
        return false;
    }

    // DMA-BUF import is an EGL extension; without it there is nothing to do here.
    const char* extensions = egl_->fp_eglQueryString(egl_->display, EGL_EXTENSIONS);
    if (!extensions || !strstr(extensions, "EGL_EXT_image_dma_buf_import"))
    {
        LOG(WARNING) << "EGL_EXT_image_dma_buf_import is not supported";
        return false;
    }
    egl_->modifiers_supported = strstr(extensions, "EGL_EXT_image_dma_buf_import_modifiers");

    if (!egl_->fp_eglBindAPI(EGL_OPENGL_API))
    {
        LOG(WARNING) << "eglBindAPI failed:" << egl_->fp_eglGetError();
        return false;
    }

    // Prefer a config-less context; if the extension is missing, pick a config explicitly.
    EGLConfig config = EGL_NO_CONFIG_KHR;
    if (!strstr(extensions, "EGL_KHR_no_config_context"))
    {
        const EGLint config_attribs[] = {
            EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
            EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
            EGL_NONE
        };

        EGLint num_configs = 0;
        if (!egl_->fp_eglChooseConfig(egl_->display, config_attribs, &config, 1, &num_configs) ||
            num_configs < 1)
        {
            LOG(WARNING) << "eglChooseConfig failed:" << egl_->fp_eglGetError();
            return false;
        }
    }

    egl_->context = egl_->fp_eglCreateContext(egl_->display, config, EGL_NO_CONTEXT, nullptr);
    if (egl_->context == EGL_NO_CONTEXT)
    {
        LOG(WARNING) << "eglCreateContext failed:" << egl_->fp_eglGetError();
        return false;
    }

    // Do NOT make the context current here: this runs on the capture thread, but the context is used
    // (and made current) on the PipeWire thread in imageFromDmaBuf(). An EGL context may be current
    // on only one thread, so binding it here would make the PipeWire-thread eglMakeCurrent fail.
    LOG(INFO) << "EGL DMA-BUF import initialized (EGL" << major << "." << minor << ")";
    return true;
}
