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

#include "base/codec/multi_channel_resampler.h"

#include "base/logging.h"
#include "base/codec/audio_bus.h"

#include <algorithm>
#include <cstring>
#include <memory>

namespace base {

//--------------------------------------------------------------------------------------------------
MultiChannelResampler::MultiChannelResampler(int channels,
                                             double io_sample_rate_ratio,
                                             size_t request_size,
                                             const ReadCB& read_cb)
    : read_cb_(read_cb),
      wrapped_resampler_audio_bus_(AudioBus::CreateWrapper(channels)),
      output_frames_ready_(0)
{
    // Allocate each channel's resampler.
    resamplers_.reserve(static_cast<size_t>(channels));
    for (int i = 0; i < channels; ++i)
    {
        resamplers_.emplace_back(std::make_unique<SincResampler>(
            io_sample_rate_ratio, static_cast<int>(request_size),
            std::bind(&MultiChannelResampler::ProvideInput,
                this, i, std::placeholders::_1, std::placeholders::_2)));
    }

    // Setup the wrapped AudioBus for channel data.
    wrapped_resampler_audio_bus_->set_frames(static_cast<int>(request_size));

    // Allocate storage for all channels except the first, which will use the
    // |destination| provided to ProvideInput() directly.
    if (channels > 1)
    {
        resampler_audio_bus_ = AudioBus::Create(channels - 1, static_cast<int>(request_size));
        for (int i = 0; i < resampler_audio_bus_->channels(); ++i)
        {
            wrapped_resampler_audio_bus_->SetChannelData(i + 1, resampler_audio_bus_->channel(i));
        }
    }
}

//--------------------------------------------------------------------------------------------------
MultiChannelResampler::~MultiChannelResampler() = default;

//--------------------------------------------------------------------------------------------------
void MultiChannelResampler::Resample(int frames, AudioBus* audio_bus)
{
    DCHECK_EQ(static_cast<size_t>(audio_bus->channels()), resamplers_.size());

    // Optimize the single channel case to avoid the chunking process below.
    if (audio_bus->channels() == 1)
    {
        resamplers_[0]->Resample(frames, audio_bus->channel(0));
        return;
    }

    // We need to ensure that SincResampler only calls ProvideInput once for each
    // channel.  To ensure this, we chunk the number of requested frames into
    // SincResampler::ChunkSize() sized chunks.  SincResampler guarantees it will
    // only call ProvideInput() once when we resample this way.
    output_frames_ready_ = 0;
    while (output_frames_ready_ < frames)
    {
        int chunk_size = resamplers_[0]->ChunkSize();
        int frames_this_time = std::min(frames - output_frames_ready_, chunk_size);

        // Resample each channel.
        for (size_t i = 0; i < resamplers_.size(); ++i)
        {
            DCHECK_EQ(chunk_size, resamplers_[i]->ChunkSize());

            // Depending on the sample-rate scale factor, and the internal buffering
            // used in a SincResampler kernel, this call to Resample() will only
            // sometimes call ProvideInput().  However, if it calls ProvideInput() for
            // the first channel, then it will call it for the remaining channels,
            // since they all buffer in the same way and are processing the same
            // number of frames.
            resamplers_[i]->Resample(
                frames_this_time, audio_bus->channel(static_cast<int>(i)) + output_frames_ready_);
        }

        output_frames_ready_ += frames_this_time;
    }
}

//--------------------------------------------------------------------------------------------------
void MultiChannelResampler::ProvideInput(int channel, int frames, float* destination)
{
    // Get the data from the multi-channel provider when the first channel asks
    // for it.  For subsequent channels, we can just dish out the channel data
    // from that (stored in |resampler_audio_bus_|).
    if (channel == 0)
    {
        wrapped_resampler_audio_bus_->SetChannelData(0, destination);
        read_cb_(output_frames_ready_, wrapped_resampler_audio_bus_.get());
    }
    else
    {
        // All channels must ask for the same amount.  This should always be the
        // case, but let's just make sure.
        DCHECK_EQ(frames, wrapped_resampler_audio_bus_->frames());

        // Copy the channel data from what we received from |read_cb_|.
        memcpy(destination, wrapped_resampler_audio_bus_->channel(channel),
               sizeof(*wrapped_resampler_audio_bus_->channel(channel)) * frames);
    }
}

//--------------------------------------------------------------------------------------------------
void MultiChannelResampler::Flush()
{
    for (size_t i = 0; i < resamplers_.size(); ++i)
        resamplers_[i]->Flush();
}

//--------------------------------------------------------------------------------------------------
void MultiChannelResampler::SetRatio(double io_sample_rate_ratio)
{
    for (size_t i = 0; i < resamplers_.size(); ++i)
        resamplers_[i]->SetRatio(io_sample_rate_ratio);
}

//--------------------------------------------------------------------------------------------------
int MultiChannelResampler::ChunkSize() const
{
    DCHECK(!resamplers_.empty());
    return resamplers_[0]->ChunkSize();
}

//--------------------------------------------------------------------------------------------------
double MultiChannelResampler::BufferedFrames() const
{
    DCHECK(!resamplers_.empty());
    return resamplers_[0]->BufferedFrames();
}

//--------------------------------------------------------------------------------------------------
void MultiChannelResampler::PrimeWithSilence()
{
    DCHECK(!resamplers_.empty());
    for (size_t i = 0; i < resamplers_.size(); ++i)
        resamplers_[i]->PrimeWithSilence();
}

} // namespace base
