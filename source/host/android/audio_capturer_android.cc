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

#include "host/android/audio_capturer_android.h"

#include <QCoreApplication>
#include <QJniEnvironment>
#include <QJniObject>
#include <QMutex>

#include "base/logging.h"
#include "base/codec/audio_encoder.h"

namespace {

const char kMediaProjectionClass[] = "org/aspia/host/MediaProjection";

// Guards the single live capturer against the JNI frame callback (on the Java reader thread), which must
// never touch a destroyed instance. At most one capturer exists, so a bare pointer is enough.
QMutex g_mutex;
AudioCapturerAndroid* g_instance = nullptr;
bool g_registered = false;

} // namespace

//--------------------------------------------------------------------------------------------------
AudioCapturerAndroid::AudioCapturerAndroid(QObject* parent)
    : QObject(parent)
{
    QMutexLocker locker(&g_mutex);

    if (!g_registered)
    {
        const JNINativeMethod methods[] =
        {
            {"nativeOnAudioFrame", "(Ljava/nio/ByteBuffer;I)V", reinterpret_cast<void*>(nativeOnAudioFrame)}
        };

        QJniEnvironment env;
        if (env.registerNativeMethods(kMediaProjectionClass, methods, std::size(methods)))
            g_registered = true;
        else
            LOG(ERROR) << "Unable to register audio capture native methods";
    }

    g_instance = this;
}

//--------------------------------------------------------------------------------------------------
AudioCapturerAndroid::~AudioCapturerAndroid()
{
    QJniObject::callStaticMethod<void>(kMediaProjectionClass, "stopAudioCapture", "()V");

    QMutexLocker locker(&g_mutex);
    if (g_instance == this)
        g_instance = nullptr;
}

//--------------------------------------------------------------------------------------------------
bool AudioCapturerAndroid::start()
{
    QJniObject context = QNativeInterface::QAndroidApplication::context();
    if (!context.isValid())
        return false;

    encoder_ = std::make_unique<AudioEncoder>();

    // The capture format is fixed, so set the constant packet fields once; onAudioFrame only swaps the data.
    raw_packet_.set_encoding(proto::audio::ENCODING_RAW);
    raw_packet_.set_sampling_rate(proto::audio::Packet::SAMPLING_RATE_48000);
    raw_packet_.set_bytes_per_sample(proto::audio::Packet::BYTES_PER_SAMPLE_2);
    raw_packet_.set_channels(proto::audio::Packet::CHANNELS_STEREO);

    // Playback capture reuses the screen capture MediaProjection; it runs on its own Java reader thread.
    QJniObject::callStaticMethod<void>(kMediaProjectionClass, "startAudioCapture",
                                       "(Landroid/content/Context;)V", context.object());
    return true;
}

//--------------------------------------------------------------------------------------------------
// static
void JNICALL AudioCapturerAndroid::nativeOnAudioFrame(JNIEnv* env, jclass /* clazz */, jobject buffer, jint size)
{
    // Hold |g_mutex| across the whole call. The destructor clears |g_instance| under the same lock, so it
    // blocks until an in-flight frame finishes and later frames find no instance.
    QMutexLocker locker(&g_mutex);

    if (!g_instance || !buffer || size <= 0)
        return;

    const void* data = env->GetDirectBufferAddress(buffer);
    if (!data)
        return;

    g_instance->onAudioFrame(data, size);
}

//--------------------------------------------------------------------------------------------------
void AudioCapturerAndroid::onAudioFrame(const void* data, int size)
{
    if (!encoder_)
        return;

    raw_packet_.clear_data();
    raw_packet_.add_data(data, static_cast<size_t>(size));

    // Encode on the Java reader thread; the queued signal hops the finished buffer to the agent thread.
    // newMessage() clears the reused message before the encoder fills it.
    proto::audio::HostToClient& message = outgoing_message_.newMessage<proto::audio::HostToClient>();
    if (!encoder_->encode(raw_packet_, message.mutable_packet()))
        return;

    emit sig_audioPacket(outgoing_message_.serialize<proto::audio::HostToClient>());
}
