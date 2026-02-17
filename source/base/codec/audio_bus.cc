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

#include "base/codec/audio_bus.h"

#include "base/logging.h"
#include "base/codec/vector_math.h"

#include <cstring>

namespace base {

//--------------------------------------------------------------------------------------------------
static bool IsAligned(void* ptr)
{
    return (reinterpret_cast<uintptr_t>(ptr) & (AudioBus::kChannelAlignment - 1)) == 0U;
}

//--------------------------------------------------------------------------------------------------
// In order to guarantee that the memory block for each channel starts at an
// aligned address when splitting a contiguous block of memory into one block
// per channel, we may have to make these blocks larger than otherwise needed.
// We do this by allocating space for potentially more frames than requested.
// This method returns the required size for the contiguous memory block
// in bytes and outputs the adjusted number of frames via |out_aligned_frames|.
static int CalculateMemorySizeInternal(int channels,
                                       int frames,
                                       int* out_aligned_frames)
{
    // Since our internal sample format is float, we can guarantee the alignment
    // by making the number of frames an integer multiple of
    // AudioBus::kChannelAlignment / sizeof(float).
    int aligned_frames =
        ((frames * sizeof(float) + AudioBus::kChannelAlignment - 1) &
        ~(AudioBus::kChannelAlignment - 1)) / sizeof(float);

    if (out_aligned_frames)
        *out_aligned_frames = aligned_frames;

    return sizeof(float) * channels * aligned_frames;
}

//--------------------------------------------------------------------------------------------------
static void ValidateConfig(int channels, int frames)
{
    CHECK_GT(frames, 0);
    CHECK_GT(channels, 0);

    static const int kMaxChannels = 32;
    CHECK_LE(channels, kMaxChannels);
}

//--------------------------------------------------------------------------------------------------
void AudioBus::CheckOverflow(int start_frame, int frames, int total_frames)
{
    CHECK_GE(start_frame, 0);
    CHECK_GE(frames, 0);
    CHECK_GT(total_frames, 0);
    int sum = start_frame + frames;
    CHECK_LE(sum, total_frames);
    CHECK_GE(sum, 0);
}

//--------------------------------------------------------------------------------------------------
AudioBus::AudioBus(int channels, int frames)
    : frames_(frames),
      can_set_channel_data_(false)
{
    ValidateConfig(channels, frames_);

    int aligned_frames = 0;
    int size = CalculateMemorySizeInternal(channels, frames, &aligned_frames);

    data_.reset(static_cast<float*>(base::alignedAlloc(size, AudioBus::kChannelAlignment)));

    BuildChannelData(channels, aligned_frames, data_.get());
}

//--------------------------------------------------------------------------------------------------
AudioBus::AudioBus(int channels, int frames, float* data)
    : frames_(frames),
      can_set_channel_data_(false)
{
    // Since |data| may have come from an external source, ensure it's valid.
    CHECK(data);
    ValidateConfig(channels, frames_);

    int aligned_frames = 0;
    CalculateMemorySizeInternal(channels, frames, &aligned_frames);

    BuildChannelData(channels, aligned_frames, data);
}

//--------------------------------------------------------------------------------------------------
AudioBus::AudioBus(int frames, const QVector<float*>& channel_data)
    : channel_data_(channel_data),
      frames_(frames),
      can_set_channel_data_(false)
{
    ValidateConfig(static_cast<int>(channel_data_.size()), frames_);

    // Sanity check wrapped vector for alignment and channel count.
    for (int i = 0; i < channel_data_.size(); ++i)
        DCHECK(IsAligned(channel_data_[i]));
}

//--------------------------------------------------------------------------------------------------
AudioBus::AudioBus(int channels)
    : channel_data_(channels),
      frames_(0),
      can_set_channel_data_(true)
{
    CHECK_GT(channels, 0);
    for (int i = 0; i < channel_data_.size(); ++i)
        channel_data_[i] = NULL;
}

//--------------------------------------------------------------------------------------------------
AudioBus::~AudioBus() = default;

//--------------------------------------------------------------------------------------------------
std::unique_ptr<AudioBus> AudioBus::Create(int channels, int frames)
{
    return std::unique_ptr<AudioBus>(new AudioBus(channels, frames));
}

//--------------------------------------------------------------------------------------------------
std::unique_ptr<AudioBus> AudioBus::CreateWrapper(int channels)
{
    return std::unique_ptr<AudioBus>(new AudioBus(channels));
}

//--------------------------------------------------------------------------------------------------
std::unique_ptr<AudioBus> AudioBus::WrapVector(int frames, const QVector<float*>& channel_data)
{
    return std::unique_ptr<AudioBus>(new AudioBus(frames, channel_data));
}

//--------------------------------------------------------------------------------------------------
std::unique_ptr<AudioBus> AudioBus::WrapMemory(int channels,
                                               int frames,
                                               void* data)
{
    // |data| must be aligned by AudioBus::kChannelAlignment.
    CHECK(IsAligned(data));
    return std::unique_ptr<AudioBus>(new AudioBus(channels, frames, static_cast<float*>(data)));
}

//--------------------------------------------------------------------------------------------------
std::unique_ptr<const AudioBus> AudioBus::WrapReadOnlyMemory(int channels,
                                                             int frames,
                                                             const void* data)
{
    // Note: const_cast is generally dangerous but is used in this case since
    // AudioBus accomodates both read-only and read/write use cases. A const
    // AudioBus object is returned to ensure no one accidentally writes to the
    // read-only data.
    return WrapMemory(channels, frames, const_cast<void*>(data));
}

//--------------------------------------------------------------------------------------------------
void AudioBus::SetChannelData(int channel, float* data)
{
    CHECK(can_set_channel_data_);
    CHECK(data);
    CHECK_GE(channel, 0);
    CHECK_LT(static_cast<qsizetype>(channel), channel_data_.size());
    DCHECK(IsAligned(data));
    channel_data_[channel] = data;
}

//--------------------------------------------------------------------------------------------------
void AudioBus::set_frames(int frames)
{
    CHECK(can_set_channel_data_);
    ValidateConfig(static_cast<int>(channel_data_.size()), frames);
    frames_ = frames;
}

//--------------------------------------------------------------------------------------------------
size_t AudioBus::GetBitstreamDataSize() const
{
    DCHECK(is_bitstream_format_);
    return bitstream_data_size_;
}

//--------------------------------------------------------------------------------------------------
void AudioBus::SetBitstreamDataSize(size_t data_size)
{
    DCHECK(is_bitstream_format_);
    bitstream_data_size_ = data_size;
}

//--------------------------------------------------------------------------------------------------
int AudioBus::GetBitstreamFrames() const
{
    DCHECK(is_bitstream_format_);
    return bitstream_frames_;
}

//--------------------------------------------------------------------------------------------------
void AudioBus::SetBitstreamFrames(int frames)
{
    DCHECK(is_bitstream_format_);
    bitstream_frames_ = frames;
}

//--------------------------------------------------------------------------------------------------
void AudioBus::ZeroFramesPartial(int start_frame, int frames)
{
    CheckOverflow(start_frame, frames, frames_);

    if (frames <= 0)
        return;

    if (is_bitstream_format_)
    {
        // No need to clean unused region for bitstream formats.
        if (start_frame >= bitstream_frames_)
            return;

        // Cannot clean partial frames.
        DCHECK_EQ(start_frame, 0);
        DCHECK(frames >= bitstream_frames_);

        // For compressed bitstream, zeroed buffer is not valid and would be
        // discarded immediately. It is faster and makes more sense to reset
        // |bitstream_data_size_| and |is_bitstream_format_| so that the buffer
        // contains no data instead of zeroed data.
        SetBitstreamDataSize(0);
        SetBitstreamFrames(0);
        return;
    }

    for (int i = 0; i < channel_data_.size(); ++i)
    {
        memset(channel_data_[i] + start_frame, 0, frames * sizeof(*channel_data_[i]));
    }
}

//--------------------------------------------------------------------------------------------------
void AudioBus::ZeroFrames(int frames)
{
    ZeroFramesPartial(0, frames);
}

//--------------------------------------------------------------------------------------------------
void AudioBus::Zero()
{
    ZeroFrames(frames_);
}

//--------------------------------------------------------------------------------------------------
bool AudioBus::AreFramesZero() const
{
    DCHECK(!is_bitstream_format_);
    for (int i = 0; i < channel_data_.size(); ++i)
    {
        for (int j = 0; j < frames_; ++j)
        {
            if (channel_data_[i][j])
                return false;
        }
    }
    return true;
}

//--------------------------------------------------------------------------------------------------
int AudioBus::CalculateMemorySize(int channels, int frames)
{
    return CalculateMemorySizeInternal(channels, frames, NULL);
}

//--------------------------------------------------------------------------------------------------
void AudioBus::BuildChannelData(int channels, int aligned_frames, float* data)
{
    DCHECK(!is_bitstream_format_);
    DCHECK(IsAligned(data));
    DCHECK_EQ(channel_data_.size(), 0U);
    // Initialize |channel_data_| with pointers into |data|.
    channel_data_.reserve(channels);
    for (int i = 0; i < channels; ++i)
        channel_data_.emplace_back(data + i * aligned_frames);
}

//--------------------------------------------------------------------------------------------------
void AudioBus::CopyTo(AudioBus* dest) const
{
    dest->set_is_bitstream_format(is_bitstream_format());
    if (is_bitstream_format())
    {
        dest->SetBitstreamDataSize(GetBitstreamDataSize());
        dest->SetBitstreamFrames(GetBitstreamFrames());
        memcpy(dest->channel(0), channel(0), GetBitstreamDataSize());
        return;
    }

    CopyPartialFramesTo(0, frames(), 0, dest);
}

//--------------------------------------------------------------------------------------------------
void AudioBus::CopyPartialFramesTo(int source_start_frame,
                                   int frame_count,
                                   int dest_start_frame,
                                   AudioBus* dest) const
{
    DCHECK(!is_bitstream_format_);
    CHECK_EQ(channels(), dest->channels());
    CHECK_LE(source_start_frame + frame_count, frames());
    CHECK_LE(dest_start_frame + frame_count, dest->frames());

    // Since we don't know if the other AudioBus is wrapped or not (and we don't
    // want to care), just copy using the public channel() accessors.
    for (int i = 0; i < channels(); ++i)
    {
        memcpy(dest->channel(i) + dest_start_frame,
               channel(i) + source_start_frame,
               sizeof(*channel(i)) * frame_count);
    }
}

//--------------------------------------------------------------------------------------------------
void AudioBus::Scale(float volume)
{
    DCHECK(!is_bitstream_format_);
    if (volume > 0 && volume != 1)
    {
        for (int i = 0; i < channels(); ++i)
        {
            float* ch = channel(i);
            FMUL(ch, volume, frames(), ch);
        }
    }
    else if (volume == 0)
    {
        Zero();
    }
}

//--------------------------------------------------------------------------------------------------
void AudioBus::SwapChannels(int a, int b)
{
    DCHECK(!is_bitstream_format_);
    DCHECK(a < channels() && a >= 0);
    DCHECK(b < channels() && b >= 0);
    DCHECK_NE(a, b);
    std::swap(channel_data_[a], channel_data_[b]);
}

} // namespace base
