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

#ifndef HOST_ANDROID_AUDIO_CAPTURER_ANDROID_H
#define HOST_ANDROID_AUDIO_CAPTURER_ANDROID_H

#include <QByteArray>
#include <QObject>

#include <jni.h>

#include <memory>

#include "base/serialization.h"
#include "proto/desktop_audio.h"

class AudioEncoder;

// Captures the system audio played on the device through the screen-capture MediaProjection, encodes it to
// Opus and emits ready-to-send desktop-audio packets. PCM is fed from a Java reader thread; the encoded
// packet reaches the owning agent through a queued signal.
class AudioCapturerAndroid final : public QObject
{
    Q_OBJECT

public:
    explicit AudioCapturerAndroid(QObject* parent = nullptr);
    ~AudioCapturerAndroid() final;

    bool start();

signals:
    // A serialized proto::audio::HostToClient carrying the encoded packet, ready to broadcast to clients.
    void sig_audioPacket(const QByteArray& packet);

private:
    // JNI trampoline invoked on the Java reader thread; routes the frame to the live capturer, guarded so
    // it never touches a destroyed instance.
    static void JNICALL nativeOnAudioFrame(JNIEnv* env, jclass clazz, jobject buffer, jint size);

    // Encodes a chunk of interleaved 16-bit stereo PCM and emits the ready packet.
    void onAudioFrame(const void* data, int size);

    std::unique_ptr<AudioEncoder> encoder_;
    proto::audio::Packet raw_packet_;
    Serializer<proto::audio::HostToClient> outgoing_message_;

    Q_DISABLE_COPY_MOVE(AudioCapturerAndroid)
};

#endif // HOST_ANDROID_AUDIO_CAPTURER_ANDROID_H
