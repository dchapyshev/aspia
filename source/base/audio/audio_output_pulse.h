//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef BASE__AUDIO__AUDIO_OUTPUT_PULSE_H
#define BASE__AUDIO__AUDIO_OUTPUT_PULSE_H

#include "base/macros_magic.h"
#include "base/audio/audio_output.h"

namespace base {

class AudioOutputPulse : public AudioOutput
{
public:
    explicit AudioOutputPulse(const NeedMoreDataCB& need_more_data_cb);
    ~AudioOutputPulse();

    // AudioOutput implementation.
    bool start() override;
    bool stop() override;

private:
    DISALLOW_COPY_AND_ASSIGN(AudioOutputPulse);
};

} // namespace base

#endif // BASE__AUDIO__AUDIO_OUTPUT_PULSE_H
