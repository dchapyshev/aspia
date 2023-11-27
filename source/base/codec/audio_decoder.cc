//
// Aspia Project
// Copyright (C) 2016-2023 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/codec/audio_decoder.h"

#include "base/logging.h"
#include "base/codec/audio_decoder_opus.h"

namespace base {

//--------------------------------------------------------------------------------------------------
std::unique_ptr<AudioDecoder> AudioDecoder::create(proto::AudioEncoding encoding)
{
    if (encoding == proto::AUDIO_ENCODING_OPUS)
        return std::unique_ptr<AudioDecoder>(new AudioDecoderOpus());

    NOTIMPLEMENTED();
    return nullptr;
}

} // namespace base
