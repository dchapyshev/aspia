//
// Aspia Project
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

#ifndef BASE_AUDIO_LINUX_AUDIO_PIPE_READER_H
#define BASE_AUDIO_LINUX_AUDIO_PIPE_READER_H

#include "base/task_runner.h"
#include "base/waitable_timer.h"
#include "base/files/file_path_watcher.h"
#include "base/files/file_descriptor_watcher_posix.h"
#include "base/memory/local_memory.h"
#include "proto/desktop.h"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

#include <unistd.h>

namespace base {

class AudioPipeReader : public base::enable_shared_from_this<AudioPipeReader>
{
public:
    ~AudioPipeReader();

    // PulseAudio's module-pipe-sink must be configured to use the following
    // parameters for the sink we read from.
    static const proto::AudioPacket::SamplingRate kSamplingRate =
        proto::AudioPacket::SAMPLING_RATE_48000;
    static const proto::AudioPacket::BytesPerSample kBytesPerSample =
        proto::AudioPacket::BYTES_PER_SAMPLE_2;
    static const proto::AudioPacket::Channels kChannels = proto::AudioPacket::CHANNELS_STEREO;

    class Delegate
    {
    public:
        virtual ~Delegate() = default;

        virtual void onDataRead(const std::string& data) = 0;
    };

    static local_shared_ptr<AudioPipeReader> create(std::shared_ptr<TaskRunner> task_runner,
                                                    const std::filesystem::path& pipe_path,
                                                    Delegate* delegate);

    void start();

private:
    AudioPipeReader(std::shared_ptr<TaskRunner> task_runner,
                    const std::filesystem::path& pipe_path,
                    Delegate* delegate);

    void onDirectoryChanged(const std::filesystem::path& path, bool error);
    void tryOpenPipe();
    void startTimer();
    void doCapture();
    void waitForPipeReadable();

    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = std::chrono::time_point<Clock>;
    using Milliseconds = std::chrono::milliseconds;
    using Seconds = std::chrono::seconds;

    class ScopedFile
    {
    public:
        ScopedFile() = default;
        explicit ScopedFile(int fd)
            : fd_(fd)
        {
            // Nothing
        }

        ~ScopedFile()
        {
            reset();
        }

        ScopedFile(ScopedFile& other) noexcept
        {
            reset(other.fd_);
            other.reset();
        }

        ScopedFile& operator=(ScopedFile&& other) noexcept
        {
            reset(other.fd_);
            other.reset();
            return *this;
        }

        int get() { return fd_; }

        void reset(int fd = -1)
        {
            if (fd_ != -1)
            {
                close(fd_);
                fd_ = -1;
            }

            fd_ = fd;
        }

    private:
        int fd_ = -1;
        Q_DISABLE_COPY(ScopedFile)
    };

    std::shared_ptr<TaskRunner> task_runner_;
    std::filesystem::path pipe_path_;
    Delegate* delegate_;

    // Watcher for the directory that contains audio pipe we are reading from, to monitor when
    // pulseaudio creates or deletes it.
    base::FilePathWatcher file_watcher_;

    ScopedFile pipe_;
    base::WaitableTimer timer_;

    // Size of the pipe buffer.
    int pipe_buffer_size_;

    // Period between pipe reads.
    Milliseconds capture_period_;

    // Time when capturing was started.
    TimePoint started_time_;

    // Stream position of the last capture in bytes with zero position
    // corresponding to |started_time_|. Must always be a multiple of the sample
    // size.
    int64_t last_capture_position_;

    // Bytes left from the previous read.
    std::string left_over_bytes_;

    std::unique_ptr<FileDescriptorWatcher> pipe_watch_controller_;

    Q_DISABLE_COPY(AudioPipeReader)
};

} // namespace base

#endif // BASE_AUDIO_LINUX_AUDIO_PIPE_READER_H
