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

class AudioOutputAndroid final
    : public AudioOutput,
      public oboe::AudioStreamDataCallback,
      public oboe::AudioStreamErrorCallback
{
public:
    explicit AudioOutputAndroid(const NeedMoreDataCB& need_more_data_cb);
    ~AudioOutputAndroid() final;

    // AudioOutput implementation.
    bool start() final;
    void stop() final;

    // oboe::AudioStreamDataCallback implementation.
    oboe::DataCallbackResult onAudioReady(
        oboe::AudioStream* stream, void* audio_data, int32_t num_frames) final;

    // oboe::AudioStreamErrorCallback implementation.
    void onErrorAfterClose(oboe::AudioStream* stream, oboe::Result error) final;

private:
    // Opens and starts a playback stream on the current default route. Called with |mutex_| held.
    bool openStream();

    // Guards |stream_|: start()/stop() run on the owner thread while the disconnect notification
    // arrives on an internal Oboe thread.
    QMutex mutex_;
    std::shared_ptr<oboe::AudioStream> stream_;

    Q_DISABLE_COPY(AudioOutputAndroid)
};

#endif // BASE_AUDIO_AUDIO_OUTPUT_ANDROID_H
