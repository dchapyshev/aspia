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

#include "host/screen_capturer_android.h"

#include <libyuv/convert_argb.h>

#include <QCoreApplication>
#include <QFuture>
#include <QHash>
#include <QJniEnvironment>
#include <QJniObject>
#include <QMutex>

#include "base/logging.h"
#include "base/desktop/frame_aligned.h"
#include "base/desktop/region.h"

namespace {

const char kCapturerClass[] = "org/aspia/host/ScreenCapturer";

// Frame buffers are 32-byte aligned to match the other capturers (SIMD-friendly strides).
const size_t kFrameAlignment = 32;

// Maps the token handed to the Java side back to the live capturer. Guards registration so the JNI
// callbacks, which arrive on the Android main thread and the ImageReader listener thread, never touch a
// destroyed instance.
QMutex g_mutex;
QHash<qint64, ScreenCapturerAndroid*> g_instances;
qint64 g_next_token = 0;

//--------------------------------------------------------------------------------------------------
ScreenCapturerAndroid* instanceForToken(qint64 token)
{
    QMutexLocker locker(&g_mutex);
    return g_instances.value(token, nullptr);
}

//--------------------------------------------------------------------------------------------------
void JNICALL nativeOnStarted(JNIEnv* /* env */, jclass /* clazz */, jlong token, jboolean success,
                             jint width, jint height, jint dpi)
{
    ScreenCapturerAndroid* self = instanceForToken(token);
    if (self)
        self->onStarted(success, QSize(width, height), QPoint(dpi, dpi));
}

//--------------------------------------------------------------------------------------------------
void JNICALL nativeOnFrame(JNIEnv* env, jclass /* clazz */, jlong token, jobject buffer,
                           jint width, jint height, jint /* pixel_stride */, jint row_stride)
{
    ScreenCapturerAndroid* self = instanceForToken(token);
    if (!self || !buffer)
        return;

    const quint8* src = static_cast<const quint8*>(env->GetDirectBufferAddress(buffer));
    if (!src)
        return;

    self->onFrame(src, width, height, row_stride);
}

//--------------------------------------------------------------------------------------------------
void ensureRegistered()
{
    QMutexLocker locker(&g_mutex);

    static bool registered = false;
    if (registered)
        return;

    const JNINativeMethod methods[] =
    {
        { "nativeOnStarted", "(JZIII)V", reinterpret_cast<void*>(nativeOnStarted) },
        { "nativeOnFrame", "(JLjava/nio/ByteBuffer;IIII)V", reinterpret_cast<void*>(nativeOnFrame) }
    };

    QJniEnvironment env;
    if (!env.registerNativeMethods(kCapturerClass, methods, 2))
        LOG(ERROR) << "Unable to register native methods for ScreenCapturer";
    else
        registered = true;
}

} // namespace

//--------------------------------------------------------------------------------------------------
ScreenCapturerAndroid::ScreenCapturerAndroid(QObject* parent)
    : ScreenCapturer(Type::ANDROID_MEDIA, parent)
{
    LOG(INFO) << "Ctor";

    QMutexLocker locker(&g_mutex);
    token_ = ++g_next_token;
    g_instances.insert(token_, this);
}

//--------------------------------------------------------------------------------------------------
ScreenCapturerAndroid::~ScreenCapturerAndroid()
{
    LOG(INFO) << "Dtor";

    stop();

    QMutexLocker locker(&g_mutex);
    g_instances.remove(token_);
}

//--------------------------------------------------------------------------------------------------
// static
ScreenCapturerAndroid* ScreenCapturerAndroid::create(QObject* parent)
{
    ensureRegistered();

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
    // Querying the display geometry runs on the Android main thread and returns synchronously; the
    // projection itself needs user consent, so requestCapture() only launches the consent flow and
    // frames start arriving later (through nativeOnFrame), after the foreground service starts it.
    ScreenCapturerAndroid* self = this;
    const qint64 token = token_;

    QNativeInterface::QAndroidApplication::runOnAndroidMainThread([self, token]() -> QVariant
    {
        QJniObject context = QNativeInterface::QAndroidApplication::context();
        if (!context.isValid())
            return QVariant();

        const int width = QJniObject::callStaticMethod<jint>(
            kCapturerClass, "displayWidth", "(Landroid/content/Context;)I", context.object());
        const int height = QJniObject::callStaticMethod<jint>(
            kCapturerClass, "displayHeight", "(Landroid/content/Context;)I", context.object());
        const int dpi = QJniObject::callStaticMethod<jint>(
            kCapturerClass, "displayDpi", "(Landroid/content/Context;)I", context.object());

        self->start_size_ = QSize(width, height);
        self->start_dpi_ = QPoint(dpi, dpi);

        QJniObject::callStaticMethod<void>(kCapturerClass, "requestCapture",
            "(Landroid/content/Context;J)V", context.object(), static_cast<jlong>(token));
        return QVariant();
    }).waitForFinished();

    if (start_size_.isEmpty())
        return false;

    active_ = true;
    screen_rect_ = QRect(QPoint(0, 0), start_size_);
    dpi_ = start_dpi_;
    return true;
}

//--------------------------------------------------------------------------------------------------
void ScreenCapturerAndroid::stop()
{
    if (!active_)
        return;

    active_ = false;

    const qint64 token = token_;
    QNativeInterface::QAndroidApplication::runOnAndroidMainThread([token]() -> QVariant
    {
        QJniObject context = QNativeInterface::QAndroidApplication::context();
        if (!context.isValid())
            return QVariant();

        QJniObject::callStaticMethod<void>(kCapturerClass, "stopCapture",
            "(Landroid/content/Context;J)V", context.object(), static_cast<jlong>(token));
        return QVariant();
    }).waitForFinished();

    QMutexLocker locker(&frame_mutex_);
    queue_.reset();
    has_new_frame_ = false;
}

//--------------------------------------------------------------------------------------------------
void ScreenCapturerAndroid::onStarted(bool success, const QSize& size, const QPoint& dpi)
{
    QMutexLocker locker(&frame_mutex_);

    if (!success)
    {
        // Consent was declined or the projection could not start; captureFrame() will report it.
        consent_failed_ = true;
        return;
    }

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
