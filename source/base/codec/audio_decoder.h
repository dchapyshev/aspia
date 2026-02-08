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

#ifndef BASE_CODEC_AUDIO_DECODER_H
#define BASE_CODEC_AUDIO_DECODER_H

#include <memory>

#include "proto/desktop.h"

struct OpusDecoder;

namespace base {

class AudioDecoder
{
public:
    AudioDecoder();
    ~AudioDecoder();

    std::unique_ptr<proto::desktop::AudioPacket> decode(const proto::desktop::AudioPacket& packet);

private:
    void initDecoder();
    void destroyDecoder();
    bool resetForPacket(const proto::desktop::AudioPacket& packet);

    int sampling_rate_ = 0;
    int channels_ = 0;
    OpusDecoder* decoder_ = nullptr;

    Q_DISABLE_COPY(AudioDecoder)
};

} // namespace base

#endif // BASE_CODEC_AUDIO_DECODER_H
