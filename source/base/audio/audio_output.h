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

#ifndef BASE_AUDIO_AUDIO_OUTPUT_H
#define BASE_AUDIO_AUDIO_OUTPUT_H

#include <QObject>

#include <functional>
#include <memory>

namespace base {

class AudioOutput : public QObject
{
    Q_OBJECT

public:
    virtual ~AudioOutput() = default;

    static const size_t kSampleRate = 48000;
    static const size_t kBitsPerSample = 16;
    static const size_t kBytesPerSample = 2;
    static const size_t kChannels = 2;

    using NeedMoreDataCB = std::function<size_t(void* data, size_t size)>;

    static std::unique_ptr<AudioOutput> create(const NeedMoreDataCB& need_more_data_cb);

    virtual bool start() = 0;
    virtual bool stop() = 0;

protected:
    explicit AudioOutput(const NeedMoreDataCB& need_more_data_cb);
    void onDataRequest(qint16* audio_samples, size_t audio_samples_count);

private:
    NeedMoreDataCB need_more_data_cb_;
};

} // namespace base

#endif // BASE_AUDIO_AUDIO_OUTPUT_H
