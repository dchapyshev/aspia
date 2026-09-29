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

#include "host/android/screen_capturer_android.h"

#include <libyuv/convert_argb.h>

#include <QCoreApplication>
#include <QJniEnvironment>
#include <QJniObject>
#include <QMutex>

#include "base/logging.h"
#include "base/desktop/frame_aligned.h"
#include "base/desktop/region.h"
#include "host/system_settings.h"

namespace {

const char kCapturerClass[] = "org/aspia/host/MediaProjection";

// Frame buffers are 32-byte aligned to match the other capturers (SIMD-friendly strides).
const size_t kFrameAlignment = 32;

// Guards the single live capturer against the JNI callbacks (Android main thread for onStarted, ImageReader
// listener thread for onFrame), which must never touch a destroyed instance. At most one capturer exists,
// so a bare pointer is enough.
QMutex g_mutex;
ScreenCapturerAndroid* g_instance = nullptr;

} // namespace

//--------------------------------------------------------------------------------------------------
ScreenCapturerAndroid::ScreenCapturerAndroid(QObject* parent)
    : ScreenCapturer(Type::ANDROID_MEDIA, parent)
{
    LOG(INFO) << "Ctor";

    QMutexLocker locker(&g_mutex);

    static bool registered = false;
    if (!registered)
    {
        const JNINativeMethod methods[] =
        {
            { "nativeOnStarted", "(ZIII)V", reinterpret_cast<void*>(nativeOnStarted) },
            { "nativeOnFrame", "(Ljava/nio/ByteBuffer;IIII)V", reinterpret_cast<void*>(nativeOnFrame) }
        };

        QJniEnvironment env;
        if (!env.registerNativeMethods(kCapturerClass, methods, 2))
            LOG(ERROR) << "Unable to register native methods for ScreenCapturer";
        else
            registered = true;
    }

    g_instance = this;
}

//--------------------------------------------------------------------------------------------------
ScreenCapturerAndroid::~ScreenCapturerAndroid()
{
    LOG(INFO) << "Dtor";

    stop();

    QMutexLocker locker(&g_mutex);
    if (g_instance == this)
        g_instance = nullptr;
}

//--------------------------------------------------------------------------------------------------
// static
ScreenCapturerAndroid* ScreenCapturerAndroid::create(QObject* parent)
{
    std::unique_ptr<ScreenCapturerAndroid> self(new ScreenCapturerAndroid(parent));
    if (!self->start())
    {
        LOG(ERROR) << "Unable to start the MediaProjection capture";
        return nullptr;
    }

    return self.release();
}

//--------------------------------------------------------------------------------------------------
bool ScreenCapturerAndroid::start()
{
    // The Java side is called on this thread. Qt runs the calls it passes to the Android main thread
    // only while the app is on the screen, and a session often starts with the app in the background.
    // The projection itself needs user consent, so requestCapture() only launches the consent flow and
    // frames start arriving later (through nativeOnFrame), after the foreground service starts it.
    QJniObject context = QNativeInterface::QAndroidApplication::context();
    if (!context.isValid())
        return false;

    const int width = QJniObject::callStaticMethod<jint>(
        kCapturerClass, "displayWidth", "(Landroid/content/Context;)I", context.object());
    const int height = QJniObject::callStaticMethod<jint>(
        kCapturerClass, "displayHeight", "(Landroid/content/Context;)I", context.object());
    const int dpi = QJniObject::callStaticMethod<jint>(
        kCapturerClass, "displayDpi", "(Landroid/content/Context;)I", context.object());

    // The display geometry is reported before the projection is running.
    const QSize size(width, height);
    if (size.isEmpty())
        return false;

    requestCapture();

    active_ = true;
    screen_rect_ = QRect(QPoint(0, 0), size);
    dpi_ = QPoint(dpi, dpi);
    return true;
}

//--------------------------------------------------------------------------------------------------
void ScreenCapturerAndroid::stop()
{
    if (!active_)
        return;

    active_ = false;

    // On this thread for the same reason as in start(), a session may end with the app in the
    // background.
    QJniObject context = QNativeInterface::QAndroidApplication::context();
    if (context.isValid())
    {
        QJniObject::callStaticMethod<void>(kCapturerClass, "stopCapture",
            "(Landroid/content/Context;)V", context.object());
    }

    QMutexLocker locker(&frame_mutex_);
    queue_.reset();
    has_new_frame_ = false;
}

//--------------------------------------------------------------------------------------------------
// static
void ScreenCapturerAndroid::requestCapture()
{
    QJniObject context = QNativeInterface::QAndroidApplication::context();
    if (!context.isValid())
        return;

    // When enabled, the accessibility service confirms the consent dialog so the capture starts
    // without a user tap. The flag is passed down so the auto-confirm only applies to this request.
    const bool auto_confirm = SystemSettings().isCaptureAutoConfirmEnabled();

    QJniObject::callStaticMethod<void>(kCapturerClass, "requestCapture",
        "(Landroid/content/Context;Z)V", context.object(), static_cast<jboolean>(auto_confirm));
}

//--------------------------------------------------------------------------------------------------
// static
bool ScreenCapturerAndroid::isDeviceLocked()
{
    QJniObject context = QNativeInterface::QAndroidApplication::context();
    if (!context.isValid())
        return false;

    return QJniObject::callStaticMethod<jboolean>(
        kCapturerClass, "isDeviceLocked", "(Landroid/content/Context;)Z", context.object());
}

//--------------------------------------------------------------------------------------------------
// static
void JNICALL ScreenCapturerAndroid::nativeOnStarted(
    JNIEnv* /* env */, jclass /* clazz */, jboolean success, jint width, jint height, jint dpi)
{
    // Hold |g_mutex| across the whole call: the destructor clears |g_instance| under the same lock, so the
    // object cannot be freed while onStarted() runs on it.
    QMutexLocker locker(&g_mutex);

    if (g_instance)
        g_instance->onStarted(success, QSize(width, height), QPoint(dpi, dpi));
}

//--------------------------------------------------------------------------------------------------
// static
void JNICALL ScreenCapturerAndroid::nativeOnFrame(
    JNIEnv* env, jclass /* clazz */, jobject buffer, jint width, jint height, jint /* pixel_stride */,
    jint row_stride)
{
    // Hold |g_mutex| across the whole call. This callback arrives on the ImageReader listener thread and
    // must not touch a destroyed instance: the destructor clears |g_instance| under the same lock, so it
    // blocks until an in-flight frame finishes, and later frames find no instance.
    QMutexLocker locker(&g_mutex);

    if (!g_instance || !buffer)
        return;

    const quint8* src = static_cast<const quint8*>(env->GetDirectBufferAddress(buffer));
    if (!src)
        return;

    g_instance->onFrame(src, width, height, row_stride);
}

//--------------------------------------------------------------------------------------------------
void ScreenCapturerAndroid::onStarted(bool success, const QSize& size, const QPoint& dpi)
{
    QMutexLocker locker(&frame_mutex_);

    if (!success)
    {
        // The system stops a running projection when the device gets locked; the capture is requested
        // again after the unlock. Anything else (consent declined, the projection could not start or
        // was stopped by the user) is final; captureFrame() will report it.
        if (started_ && isDeviceLocked())
        {
            LOG(INFO) << "Screen capture stopped on the device lock";
            started_ = false;
            restart_pending_ = true;
            has_new_frame_ = false;
        }
        else
        {
            consent_failed_ = true;
        }
        return;
    }

    started_ = true;

    if (!size.isEmpty())
    {
        screen_rect_ = QRect(QPoint(0, 0), size);
        dpi_ = dpi;
    }
}

//--------------------------------------------------------------------------------------------------
void ScreenCapturerAndroid::onFrame(const quint8* src, int width, int height, int row_stride)
{
    if (width <= 0 || height <= 0)
        return;

    const QSize size(width, height);

    QMutexLocker locker(&frame_mutex_);

    Frame* frame = queue_.currentFrame();
    if (!frame || frame->size() != size)
    {
        queue_.replaceCurrentFrame(FrameAligned::create(size, kFrameAlignment));
        frame = queue_.currentFrame();
        if (!frame)
            return;

        frame->setCapturerType(static_cast<quint32>(type()));
        frame->setDpi(dpi_);
    }

    // MediaProjection delivers RGBA_8888 (memory order R, G, B, A); the frame stores BGRA. ABGRToARGB
    // swaps the red and blue channels while copying, honouring the padded source stride.
    libyuv::ABGRToARGB(src, row_stride, frame->frameData(), frame->stride(), width, height);

    screen_rect_ = QRect(QPoint(0, 0), size);
    has_new_frame_ = true;
}

//--------------------------------------------------------------------------------------------------
int ScreenCapturerAndroid::screenCount()
{
    return 1;
}

//--------------------------------------------------------------------------------------------------
bool ScreenCapturerAndroid::screenList(ScreenList* screens)
{
    screens->screens.clear();
    screens->resolutions.clear();

    Screen screen;
    screen.id = 0;
    screen.position = QPoint(0, 0);
    screen.resolution = screen_rect_.size();
    screen.dpi = dpi_;
    screen.is_primary = true;
    screens->screens.append(screen);
    return true;
}

//--------------------------------------------------------------------------------------------------
bool ScreenCapturerAndroid::selectScreen(ScreenId screen_id)
{
    // A single virtual display is exposed; accept it or the "no selection" sentinel.
    return screen_id == kInvalidScreenId || screen_id == 0;
}

//--------------------------------------------------------------------------------------------------
ScreenCapturer::ScreenId ScreenCapturerAndroid::currentScreen() const
{
    return 0;
}

//--------------------------------------------------------------------------------------------------
const Frame* ScreenCapturerAndroid::captureFrame(Error* error)
{
    DCHECK(error);

    QMutexLocker locker(&frame_mutex_);

    if (consent_failed_)
    {
        *error = Error::PERMANENT;
        return nullptr;
    }

    if (!started_)
    {
        // Still waiting for consent. Tell the client whether it is blocked on a locked screen (a secure
        // keyguard that only the user can dismiss) or just on the confirmation, so it can show the reason.
        const bool locked = isDeviceLocked();
        *error = locked ? Error::SCREEN_LOCKED : Error::CONFIRMATION_PENDING;

        if (restart_pending_ && !locked)
        {
            restart_pending_ = false;

            // Outside the lock: the Java side calls back into onStarted() while holding its class
            // monitor, which requestCapture() takes as well.
            locker.unlock();

            LOG(INFO) << "Device unlocked, requesting the screen capture again";
            requestCapture();
        }
        return nullptr;
    }

    if (has_new_frame_)
    {
        Frame* frame = queue_.currentFrame();
        if (!frame)
        {
            *error = Error::TEMPORARY;
            return nullptr;
        }

        // MediaProjection carries no damage information, so the whole frame is reported as updated.
        frame->updatedRegion()->clear();
        *frame->updatedRegion() += screen_rect_;

        // Advance the queue: the listener keeps writing the new current frame while the consumer
        // encodes this one (now the previous frame), so the two never touch the same buffer.
        queue_.moveToNextFrame();
        has_new_frame_ = false;

        *error = Error::SUCCEEDED;
        return frame;
    }

    // No new frame: return the last one with an empty region so it is not re-encoded.
    Frame* frame = queue_.previousFrame();
    if (!frame)
    {
        *error = Error::TEMPORARY;
        return nullptr;
    }

    frame->updatedRegion()->clear();
    *error = Error::SUCCEEDED;
    return frame;
}

//--------------------------------------------------------------------------------------------------
const MouseCursor* ScreenCapturerAndroid::captureCursor()
{
    // MediaProjection composites the pointer into the frames; there is no separate cursor.
    return nullptr;
}

//--------------------------------------------------------------------------------------------------
QPoint ScreenCapturerAndroid::cursorPosition()
{
    return QPoint();
}

//--------------------------------------------------------------------------------------------------
const QRect& ScreenCapturerAndroid::desktopRect() const
{
    return screen_rect_;
}

//--------------------------------------------------------------------------------------------------
const QRect& ScreenCapturerAndroid::currentScreenRect() const
{
    return screen_rect_;
}

//--------------------------------------------------------------------------------------------------
void ScreenCapturerAndroid::reset()
{
    QMutexLocker locker(&frame_mutex_);
    queue_.reset();
    has_new_frame_ = false;
}
