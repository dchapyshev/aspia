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

#include "base/audio/audio_output_android.h"

#include "base/logging.h"

namespace {

// Oboe would otherwise pick its own callback size. Pinning it to a 10 ms chunk matches the
// granularity of AudioOutput::onDataRequest(), which pulls decoded audio in 10 ms units.
const qint32 kFramesPer10ms = AudioOutput::kSampleRate * 10 / 1000;

} // namespace

//--------------------------------------------------------------------------------------------------
AudioOutputAndroid::AudioOutputAndroid(const NeedMoreDataCB& need_more_data_cb)
    : AudioOutput(need_more_data_cb)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
AudioOutputAndroid::~AudioOutputAndroid()
{
    stop();
}

//--------------------------------------------------------------------------------------------------
bool AudioOutputAndroid::start()
{
    if (stream_)
        return true;

    oboe::AudioStreamBuilder builder;
    builder.setDirection(oboe::Direction::Output)
           ->setPerformanceMode(oboe::PerformanceMode::LowLatency)
           ->setSharingMode(oboe::SharingMode::Shared)
           ->setFormat(oboe::AudioFormat::I16)
           ->setChannelCount(static_cast<int>(kChannels))
           ->setSampleRate(static_cast<int>(kSampleRate))
           ->setFramesPerDataCallback(kFramesPer10ms)
           ->setUsage(oboe::Usage::Media)
           ->setDataCallback(this);

    oboe::Result result = builder.openStream(stream_);
    if (result != oboe::Result::OK)
    {
        LOG(ERROR) << "Unable to open audio stream:" << oboe::convertToText(result);
        return false;
    }

    result = stream_->requestStart();
    if (result != oboe::Result::OK)
    {
        LOG(ERROR) << "Unable to start audio stream:" << oboe::convertToText(result);
        stream_->close();
        stream_.reset();
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
void AudioOutputAndroid::stop()
{
    if (!stream_)
        return;

    // close() blocks until any in-flight data callback returns, so no onAudioReady() runs afterwards.
    stream_->stop();
    stream_->close();
    stream_.reset();
}

//--------------------------------------------------------------------------------------------------
oboe::DataCallbackResult AudioOutputAndroid::onAudioReady(
    oboe::AudioStream* /* stream */, void* audio_data, int32_t num_frames)
{
    onDataRequest(static_cast<qint16*>(audio_data), static_cast<size_t>(num_frames) * kChannels);
    return oboe::DataCallbackResult::Continue;
}
