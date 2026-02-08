//
// SmartCafe Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef BASE_CODEC_MULTI_CHANNEL_RESAMPLER_H
#define BASE_CODEC_MULTI_CHANNEL_RESAMPLER_H

#include <cstddef>
#include <memory>
#include <vector>

#include "base/codec/sinc_resampler.h"

namespace base {

class AudioBus;

// MultiChannelResampler is a multi channel wrapper for SincResampler; allowing
// high quality sample rate conversion of multiple channels at once.
class MultiChannelResampler
{
public:
    // Callback type for providing more data into the resampler.  Expects AudioBus
    // to be completely filled with data upon return; zero padded if not enough
    // frames are available to satisfy the request.  |frame_delay| is the number
    // of output frames already processed and can be used to estimate delay.
    using ReadCB = std::function<void(int frame_delay, AudioBus* audio_bus)>;

    // Constructs a MultiChannelResampler with the specified |read_cb|, which is
    // used to acquire audio data for resampling.  |io_sample_rate_ratio| is the
    // ratio of input / output sample rates.  |request_frames| is the size in
    // frames of the AudioBus to be filled by |read_cb|.
    MultiChannelResampler(int channels,
                          double io_sample_rate_ratio,
                          size_t request_frames,
                          const ReadCB& read_cb);
    virtual ~MultiChannelResampler();

    // Resamples |frames| of data from |read_cb_| into AudioBus.
    void Resample(int frames, AudioBus* audio_bus);

    // Flush all buffered data and reset internal indices.  Not thread safe, do
    // not call while Resample() is in progress.
    void Flush();

    // Update ratio for all SincResamplers.  SetRatio() will cause reconstruction
    // of the kernels used for resampling.  Not thread safe, do not call while
    // Resample() is in progress.
    void SetRatio(double io_sample_rate_ratio);

    // The maximum size in frames that guarantees Resample() will only make a
    // single call to |read_cb_| for more data.
    int ChunkSize() const;

    // See SincResampler::BufferedFrames.
    double BufferedFrames() const;

    // See SincResampler::PrimeWithSilence.
    void PrimeWithSilence();

private:
    // SincResampler::ReadCB implementation.  ProvideInput() will be called for
    // each channel (in channel order) as SincResampler needs more data.
    void ProvideInput(int channel, int frames, float* destination);

    // Source of data for resampling.
    ReadCB read_cb_;

    // Each channel has its own high quality resampler.
    std::vector<std::unique_ptr<SincResampler>> resamplers_;

    // Buffers for audio data going into SincResampler from ReadCB.
    std::unique_ptr<AudioBus> resampler_audio_bus_;

    // To avoid a memcpy() on the first channel we create a wrapped AudioBus where
    // the first channel points to the |destination| provided to ProvideInput().
    std::unique_ptr<AudioBus> wrapped_resampler_audio_bus_;

    // The number of output frames that have successfully been processed during
    // the current Resample() call.
    int output_frames_ready_;

    Q_DISABLE_COPY(MultiChannelResampler)
};

} // namespace base

#endif // BASE_CODEC_MULTI_CHANNEL_RESAMPLER_H
