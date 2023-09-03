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

#include "base/audio/linux/audio_pipe_reader.h"

#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"

#include <fcntl.h>
#include <sys/stat.h>

namespace base {

namespace {

const int kSampleBytesPerSecond = AudioPipeReader::kSamplingRate *
                                  AudioPipeReader::kChannels *
                                  AudioPipeReader::kBytesPerSample;

int readAtCurrentPos(int fd, char* data, int size)
{
    DCHECK(fd != -1);

    if (size < 0)
        return -1;

    int bytes_read = 0;
    int rv;
    do
    {
        rv = HANDLE_EINTR(read(fd, data + bytes_read, size - bytes_read));
        if (rv <= 0)
            break;

        bytes_read += rv;
    } while (bytes_read < size);

    return bytes_read ? bytes_read : rv;
}

} // namespace

//--------------------------------------------------------------------------------------------------
AudioPipeReader::AudioPipeReader(std::shared_ptr<TaskRunner> task_runner,
                                 const std::filesystem::path& pipe_path,
                                 Delegate* delegate)
    : task_runner_(task_runner),
      pipe_path_(pipe_path),
      delegate_(delegate),
      file_watcher_(task_runner),
      timer_(WaitableTimer::Type::REPEATED, task_runner)
{
    LOG(LS_INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
AudioPipeReader::~AudioPipeReader()
{
    LOG(LS_INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
// static
local_shared_ptr<AudioPipeReader> AudioPipeReader::create(std::shared_ptr<TaskRunner> task_runner,
                                                          const std::filesystem::path& pipe_path,
                                                          Delegate* delegate)
{
    DCHECK(task_runner);
    DCHECK(!pipe_path.empty());
    DCHECK(delegate);

    local_shared_ptr<AudioPipeReader> pipe_reader(
        new AudioPipeReader(std::move(task_runner), pipe_path, delegate));
    return pipe_reader;
}

//--------------------------------------------------------------------------------------------------
void AudioPipeReader::start()
{
    DCHECK(task_runner_->belongsToCurrentThread());

    if (!file_watcher_.watch(pipe_path_.parent_path(), true,
                             std::bind(&AudioPipeReader::onDirectoryChanged,
                                       shared_from_this(),
                                       std::placeholders::_1,
                                       std::placeholders::_2)))
    {
        LOG(LS_ERROR) << "Failed to watch pulseaudio directory " << pipe_path_.parent_path();
    }

    tryOpenPipe();
}

//--------------------------------------------------------------------------------------------------
void AudioPipeReader::onDirectoryChanged(const std::filesystem::path& path, bool error)
{
    DCHECK(task_runner_->belongsToCurrentThread());

    if (error)
    {
        LOG(LS_ERROR) << "File watcher returned an error.";
        return;
    }

    tryOpenPipe();
}

//--------------------------------------------------------------------------------------------------
void AudioPipeReader::tryOpenPipe()
{
    DCHECK(task_runner_->belongsToCurrentThread());

    ScopedFile new_pipe(HANDLE_EINTR(open(pipe_path_.c_str(), O_RDONLY | O_NONBLOCK)));
    if (new_pipe.get() != -1 && pipe_.get() != -1)
    {
        struct stat old_stat;
        struct stat new_stat;

        if (fstat(pipe_.get(), &old_stat) == 0 &&
            fstat(new_pipe.get(), &new_stat) == 0 &&
            old_stat.st_ino == new_stat.st_ino)
        {
            return;
        }
    }

    pipe_watch_controller_.reset();
    timer_.stop();

    pipe_ = std::move(new_pipe);
    if (pipe_.get() != -1)
    {
        pipe_buffer_size_ = fpathconf(pipe_.get(), _PC_PIPE_BUF);
        if (pipe_buffer_size_ < 0)
        {
            PLOG(LS_ERROR) << "fpathconf(_PC_PIPE_BUF) failed";
            pipe_buffer_size_ = 4096;
        }

        // Read from the pipe twice per buffer length, to avoid starving the stream.
        capture_period_ = Seconds(1) * pipe_buffer_size_ / kSampleBytesPerSecond / 2;

        waitForPipeReadable();
    }
}

//--------------------------------------------------------------------------------------------------
void AudioPipeReader::startTimer()
{
    DCHECK(task_runner_->belongsToCurrentThread());
    DCHECK(pipe_watch_controller_);

    pipe_watch_controller_.reset();
    started_time_ = Clock::now();
    last_capture_position_ = 0;

    timer_.start(capture_period_, std::bind(&AudioPipeReader::doCapture, this));
}

//--------------------------------------------------------------------------------------------------
void AudioPipeReader::doCapture()
{
    DCHECK(task_runner_->belongsToCurrentThread());
    DCHECK(pipe_.get() != -1);

    // Calculate how much we need read from the pipe. Pulseaudio doesn't control
    // how much data it writes to the pipe, so we need to pace the stream.
    Milliseconds stream_position =
        std::chrono::duration_cast<Milliseconds>(Clock::now() - started_time_);
    int64_t stream_position_bytes = stream_position.count() * kSampleBytesPerSecond / 1000;
    int64_t bytes_to_read = stream_position_bytes - last_capture_position_;

    std::string data = left_over_bytes_;
    size_t pos = data.size();
    left_over_bytes_.clear();
    data.resize(pos + bytes_to_read);

    while (pos < data.size())
    {
        int read_result = readAtCurrentPos(pipe_.get(), std::data(data) + pos, data.size() - pos);
        if (read_result > 0)
        {
            pos += read_result;
        }
        else
        {
            if (read_result < 0 && errno != EWOULDBLOCK && errno != EAGAIN)
            {
                PLOG(LS_ERROR) << "read failed";
            }
            break;
        }
    }

    // Stop reading from the pipe if PulseAudio isn't writing anything.
    if (pos == 0)
    {
        waitForPipeReadable();
        return;
    }

    // Save any incomplete samples we've read for later. Each packet should
    // contain integer number of samples.
    int incomplete_samples_bytes = pos % (kChannels * kBytesPerSample);
    left_over_bytes_.assign(data, pos - incomplete_samples_bytes,
                            incomplete_samples_bytes);
    data.resize(pos - incomplete_samples_bytes);

    last_capture_position_ += data.size();
    // Normally PulseAudio will keep pipe buffer full, so we should always be able
    // to read |bytes_to_read| bytes, but in case it's misbehaving we need to make
    // sure that |stream_position_bytes| doesn't go out of sync with the current
    // stream position.
    if (stream_position_bytes - last_capture_position_ > pipe_buffer_size_)
        last_capture_position_ = stream_position_bytes - pipe_buffer_size_;
    DCHECK_LE(last_capture_position_, stream_position_bytes);

    // Dispatch asynchronous notification to the stream observers.
    if (delegate_)
        delegate_->onDataRead(data);
}

//--------------------------------------------------------------------------------------------------
void AudioPipeReader::waitForPipeReadable()
{
    timer_.stop();

    DCHECK(!pipe_watch_controller_);

    pipe_watch_controller_ = std::make_unique<FileDescriptorWatcher>();
    pipe_watch_controller_->startWatching(pipe_.get(), FileDescriptorWatcher::Mode::WATCH_READ,
        std::bind(&AudioPipeReader::startTimer, shared_from_this()));
}

} // namespace base
