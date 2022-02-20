//
// Aspia Project
// Copyright (C) 2016-2022 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef BASE_CODEC_AUDIO_DECODER_OPUS_H
#define BASE_CODEC_AUDIO_DECODER_OPUS_H

#include "base/macros_magic.h"
#include "base/codec/audio_decoder.h"

struct OpusDecoder;

namespace base {

class AudioDecoderOpus : public AudioDecoder
{
public:
    AudioDecoderOpus();
    ~AudioDecoderOpus() override;

    // AudioDecoder interface.
    std::unique_ptr<proto::AudioPacket> decode(const proto::AudioPacket& packet) override;

private:
    void initDecoder();
    void destroyDecoder();
    bool resetForPacket(const proto::AudioPacket& packet);

    int sampling_rate_ = 0;
    int channels_ = 0;
    OpusDecoder* decoder_ = nullptr;

    DISALLOW_COPY_AND_ASSIGN(AudioDecoderOpus);
};

} // namespace base

#endif // BASE_CODEC_AUDIO_DECODER_OPUS_H
