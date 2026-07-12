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

#ifndef BASE_AUDIO_AUDIO_OUTPUT_ANDROID_H
#define BASE_AUDIO_AUDIO_OUTPUT_ANDROID_H

#include "base/audio/audio_output.h"

#include <QMutex>

#include <memory>

#include <oboe/Oboe.h>

class AudioOutputAndroid final : public AudioOutput
{
public:
    explicit AudioOutputAndroid(const NeedMoreDataCB& need_more_data_cb);
    ~AudioOutputAndroid() final;

    // AudioOutput implementation.
    bool start() final;
    void stop() final;

private:
    // Receives Oboe callbacks. A separate object held by std::shared_ptr (Oboe keeps its own
    // reference), so a disconnect notification arriving on an internal Oboe thread after the
    // output is destroyed finds a live bridge with a null |owner_| instead of a freed object.
    class CallbackBridge final
        : public oboe::AudioStreamDataCallback,
          public oboe::AudioStreamErrorCallback
    {
    public:
        explicit CallbackBridge(AudioOutputAndroid* owner)
            : owner_(owner)
        {
            // Nothing
        }

        // Disconnects the bridge from the owner; called from the owner's destructor.
        void detach();

        // oboe::AudioStreamDataCallback implementation.
        oboe::DataCallbackResult onAudioReady(
            oboe::AudioStream* stream, void* audio_data, int32_t num_frames) final;

        // oboe::AudioStreamErrorCallback implementation.
        void onErrorAfterClose(oboe::AudioStream* stream, oboe::Result error) final;

    private:
        QMutex mutex_;
        AudioOutputAndroid* owner_; // Guarded by |mutex_|.

        Q_DISABLE_COPY(CallbackBridge)
    };

    // Handles a device disconnect reported by Oboe; called from the bridge.
    void onStreamDisconnected(oboe::AudioStream* stream);

    // Opens and starts a playback stream on the current default route. Called with |mutex_| held.
    bool openStream();

    // Guards |stream_|: start()/stop() run on the owner thread while the disconnect notification
    // arrives on an internal Oboe thread.
    QMutex mutex_;
    std::shared_ptr<oboe::AudioStream> stream_;
    std::shared_ptr<CallbackBridge> bridge_;

    Q_DISABLE_COPY(AudioOutputAndroid)
};

#endif // BASE_AUDIO_AUDIO_OUTPUT_ANDROID_H
