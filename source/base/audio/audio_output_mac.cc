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

#include "base/audio/audio_output_mac.h"

#include "base/logging.h"
#include "base/threading/simple_thread.h"
#include "build/build_config.h"
#include "third_party/portaudio/pa_ringbuffer.h"

#include <mach/mach.h>
#include <pthread.h>
#include <sched.h>

namespace base {

AudioOutputMac::AudioOutputMac(const NeedMoreDataCB& need_more_data_cb)
    : AudioOutput(need_more_data_cb),
      stop_event_(WaitableEvent::ResetPolicy::AUTOMATIC, WaitableEvent::InitialState::NOT_SIGNALED)
{
    memset(convert_data_, 0, sizeof(convert_data_));
    memset(&stream_format_, 0, sizeof(AudioStreamBasicDescription));
    memset(&desired_format_, 0, sizeof(AudioStreamBasicDescription));

    if (initDevice())
        initPlayout();
}

AudioOutputMac::~AudioOutputMac()
{
    stop();
    terminate();

    kern_return_t kern_err = semaphore_destroy(mach_task_self(), semaphore_);
    if (kern_err != KERN_SUCCESS)
    {
        LOG(LS_ERROR) << "semaphore_destroy() error: " << kern_err;
    }
}

bool AudioOutputMac::start()
{
    std::scoped_lock lock(lock_);

    if (!playout_initialized_)
        return false;

    if (playing_)
        return true;

    DCHECK(!worker_thread_);
    worker_thread_ = std::make_unique<SimpleThread>();
    worker_thread_->start(std::bind(&AudioOutputMac::threadRun, this));

    OSStatus err = AudioDeviceStart(output_device_id_, device_io_proc_id_);
    if (err != noErr)
    {
        LOG(LS_ERROR) << "AudioDeviceStart failed";
        return false;
    }

    LOG(LS_INFO) << "Audio playout started";
    playing_ = true;
    return true;
}

bool AudioOutputMac::stop()
{
    std::scoped_lock lock(lock_);

    if (!playout_initialized_)
        return true;

    if (playing_ && is_device_alive_)
    {
        playing_ = false;
        do_stop_ = true; // Signal to io proc to stop audio device.
        lock_.unlock();  // Cannot be under lock, risk of deadlock.
        if (!stop_event_.wait(std::chrono::seconds(2)))
        {
            std::scoped_lock crit_scoped(lock_);
            LOG(LS_ERROR) << "Timed out stopping the render IOProc."
                             "We may have failed to detect a device removal.";

            // We assume capturing on a shared device has stopped as well if the IOProc times out.
            AudioDeviceStop(output_device_id_, device_io_proc_id_);
            AudioDeviceDestroyIOProcID(output_device_id_, device_io_proc_id_);
        }
        lock_.lock();
        do_stop_ = false;
        LOG(LS_INFO) << "Playout stopped";
    }
    else
    {
        AudioDeviceDestroyIOProcID(output_device_id_, device_io_proc_id_);
        LOG(LS_INFO) << "Playout uninitialized (output device)";
    }

    // Setting this signal will allow the worker thread to be stopped.
    is_device_alive_ = false;
    if (worker_thread_)
    {
        lock_.unlock();
        worker_thread_->stop();
        worker_thread_.reset();
        lock_.lock();
    }

    AudioConverterDispose(converter_);

    // Remove listeners.
    AudioObjectPropertyAddress property_address =
        { kAudioDevicePropertyStreamFormat, kAudioDevicePropertyScopeOutput, 0 };
    AudioObjectRemovePropertyListener(
        output_device_id_, &property_address, &objectListenerProc, this);

    playout_initialized_ = false;
    playing_ = false;
    return true;
}

bool AudioOutputMac::initDevice()
{
    std::scoped_lock lock(lock_);

    if (device_initialized_)
        return true;

    // PortAudio ring buffers require an elementCount which is a power of two.
    if (render_buffer_data_.empty())
    {
        UInt32 power_of_two = 1;
        while (power_of_two < kPlayBufferSizeInSamples)
            power_of_two <<= 1;

        render_buffer_data_.resize(power_of_two);
    }

    if (!pa_render_buffer_)
    {
        pa_render_buffer_ = std::make_unique<PaUtilRingBuffer>();
        if (PaUtil_InitializeRingBuffer(pa_render_buffer_.get(), sizeof(SInt16),
                                        render_buffer_data_.size(), render_buffer_data_.data()) == -1)
        {
            LOG(LS_ERROR) << "PaUtil_InitializeRingBuffer failed";
            return false;
        }
    }

    kern_return_t kern_err = semaphore_create(mach_task_self(), &semaphore_, SYNC_POLICY_FIFO, 0);
    if (kern_err != KERN_SUCCESS)
    {
        LOG(LS_ERROR) << "semaphore_create failed: " << kern_err;
        return false;
    }

    // Setting RunLoop to NULL here instructs HAL to manage its own thread for notifications. This
    // was the default behaviour on OS X 10.5 and earlier, but now must be explicitly specified. HAL
    // would otherwise try to use the main thread to issue notifications.
    AudioObjectPropertyAddress property_address =
    {
        kAudioHardwarePropertyRunLoop,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMaster
    };
    CFRunLoopRef run_loop = nullptr;
    UInt32 size = sizeof(CFRunLoopRef);
    OSStatus err = AudioObjectSetPropertyData(
        kAudioObjectSystemObject, &property_address, 0, nullptr, size, &run_loop);
    if (err != noErr)
    {
        LOG(LS_ERROR) << "Error in AudioObjectSetPropertyData";
        return false;
    }

    // Listen for any device changes.
    property_address.mSelector = kAudioHardwarePropertyDevices;
    AudioObjectAddPropertyListener(
        kAudioObjectSystemObject, &property_address, &objectListenerProc, this);

    LOG(LS_INFO) << "Audio device initialized";
    device_initialized_ = true;
    return true;
}

void AudioOutputMac::terminate()
{
    if (!device_initialized_)
        return;

    if (playing_)
    {
        LOG(LS_ERROR) << "Playback must be stopped";
        return;
    }

    std::scoped_lock lock(lock_);

    AudioObjectPropertyAddress property_address =
    {
        kAudioHardwarePropertyDevices,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMaster
    };
    AudioObjectRemovePropertyListener(
        kAudioObjectSystemObject, &property_address, &objectListenerProc, this);
    AudioHardwareUnload();

    device_initialized_ = false;
}

bool AudioOutputMac::initPlayout()
{
    std::scoped_lock lock(lock_);

    if (playing_)
        return true;

    if (playout_initialized_)
        return true;

    AudioObjectPropertyAddress property_address =
    {
        kAudioHardwarePropertyDefaultOutputDevice,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMaster
    };

    output_device_id_ = kAudioDeviceUnknown;

    // Try to use default system device.
    UInt32 size = sizeof(AudioDeviceID);
    OSStatus err = AudioObjectGetPropertyData(
        kAudioObjectSystemObject, &property_address, 0, nullptr, &size, &output_device_id_);
    if (err != noErr)
    {
        LOG(LS_ERROR) << "AudioObjectGetPropertyData failed";
        return false;
    }

    if (output_device_id_ == kAudioDeviceUnknown)
    {
        LOG(LS_ERROR) << "No default device exists";
        return false;
    }

    PaUtil_FlushRingBuffer(pa_render_buffer_.get());

    render_delay_offset_samples_ = 0;
    is_device_alive_ = true;
    do_stop_ = false;

    // Get current stream description.
    property_address = { kAudioDevicePropertyStreamFormat, kAudioDevicePropertyScopeOutput, 0 };
    memset(&stream_format_, 0, sizeof(stream_format_));
    size = sizeof(stream_format_);
    err = AudioObjectGetPropertyData(
        output_device_id_, &property_address, 0, nullptr, &size, &stream_format_);
    if (err != noErr)
    {
        LOG(LS_ERROR) << "AudioObjectGetPropertyData failed";
        return false;
    }

    if (stream_format_.mFormatID != kAudioFormatLinearPCM)
    {
        LOG(LS_ERROR) << "Unacceptable output stream format";
        return false;
    }

    if (stream_format_.mChannelsPerFrame > kMaxDeviceChannels)
    {
        LOG(LS_ERROR) << "Too many channels on output device (mChannelsPerFrame = "
                      << stream_format_.mChannelsPerFrame << ")";
        return false;
    }

    if (stream_format_.mFormatFlags & kAudioFormatFlagIsNonInterleaved)
    {
        LOG(LS_ERROR) << "Non-interleaved audio data is not supported."
                         "AudioHardware streams should not have this format.";
        return false;
    }

    if (!setDesiredFormat())
    {
        LOG(LS_ERROR) << "setDesiredFormat failed";
        return false;
    }

    // Listen for format changes.
    property_address.mSelector = kAudioDevicePropertyStreamFormat;
    err = AudioObjectAddPropertyListener(
        output_device_id_, &property_address, &objectListenerProc, this);
    if (err != noErr)
    {
        LOG(LS_ERROR) << "AudioObjectAddPropertyListener failed";
        return false;
    }

    err = AudioDeviceCreateIOProcID(output_device_id_, deviceIOProc, this, &device_io_proc_id_);
    if (err != noErr)
    {
        LOG(LS_ERROR) << "AudioDeviceCreateIOProcID failed";
        return false;
    }

    LOG(LS_INFO) << "Playout initialized";
    playout_initialized_ = true;
    return true;
}

void AudioOutputMac::threadRun()
{
    LOG(LS_INFO) << "Audio thread started";

    const int policy = SCHED_FIFO;
    const int min_prio = sched_get_priority_min(policy);
    const int max_prio = sched_get_priority_max(policy);
    if (min_prio != -1 && max_prio != -1 && (max_prio - min_prio) > 2)
    {
        sched_param param;
        param.sched_priority = max_prio - 1;
        if (pthread_setschedparam(pthread_self(), policy, &param) != 0)
        {
            LOG(LS_ERROR) << "pthread_setschedparam failed";
        }
    }

    static const int32_t kSamplesPer10ms = kSamplesPerChannel10ms * kChannels;

    while (true)
    {
        while (PaUtil_GetRingBufferWriteAvailable(pa_render_buffer_.get()) -
               render_delay_offset_samples_ < kSamplesPer10ms)
        {
            mach_timespec_t timeout;
            timeout.tv_sec = 0;
            timeout.tv_nsec = kTimerPeriodMs;

            kern_return_t kern_err = semaphore_timedwait(semaphore_, timeout);
            if (kern_err == KERN_OPERATION_TIMED_OUT)
            {
                if (!is_device_alive_)
                {
                    // The render device is no longer alive; stop the worker thread.
                    LOG(LS_INFO) << "No renderer device";
                    return;
                }
            }
            else if (kern_err != KERN_SUCCESS)
            {
                LOG(LS_ERROR) << "semaphore_timedwait failed: " << kern_err;
            }
        }

        SInt16 play_buffer[kSamplesPer10ms];

        // Ask for new PCM data to be played out using the AudioDeviceBuffer.
        onDataRequest(play_buffer, kSamplesPer10ms);

        PaUtil_WriteRingBuffer(pa_render_buffer_.get(), play_buffer, kSamplesPer10ms);
    }
}

bool AudioOutputMac::setDesiredFormat()
{
    render_delay_offset_samples_ =
        render_buffer_data_.size() - kBuffersOut * kSamplesPerChannel10ms * kChannels;

    // Our preferred format to work with.
    desired_format_.mSampleRate = kSampleRate;
    desired_format_.mChannelsPerFrame = kChannels;
    desired_format_.mBytesPerPacket = kChannels * sizeof(SInt16);
    // In uncompressed audio, a packet is one frame.
    desired_format_.mFramesPerPacket = 1;
    desired_format_.mBytesPerFrame = kChannels * sizeof(SInt16);
    desired_format_.mBitsPerChannel = sizeof(SInt16) * 8;
    desired_format_.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger | kLinearPCMFormatFlagIsPacked;
#ifdef ARCH_CPU_BIG_ENDIAN
    desired_format_.mFormatFlags |= kLinearPCMFormatFlagIsBigEndian;
#endif
    desired_format_.mFormatID = kAudioFormatLinearPCM;

    OSStatus err = AudioConverterNew(&desired_format_, &stream_format_, &converter_);
    if (err != noErr)
    {
        LOG(LS_ERROR) << "AudioConverterNew failed";
        return false;
    }

    // Try to set buffer size to desired value set to 20ms.
    const uint16_t kPlayBufDelayFixed = 20;
    UInt32 buf_byte_count = static_cast<UInt32>(
        (stream_format_.mSampleRate / 1000.0) * kPlayBufDelayFixed *
        stream_format_.mChannelsPerFrame * sizeof(Float32));
    if (stream_format_.mFramesPerPacket != 0)
    {
        if (buf_byte_count % stream_format_.mFramesPerPacket != 0)
        {
            buf_byte_count = (static_cast<UInt32>(
                buf_byte_count / stream_format_.mFramesPerPacket) + 1) *
                stream_format_.mFramesPerPacket;
        }
    }

    // Ensure the buffer size is within the range provided by the device.
    AudioObjectPropertyAddress property_address =
        { kAudioDevicePropertyDataSource, kAudioDevicePropertyScopeOutput, 0 };
    property_address.mSelector = kAudioDevicePropertyBufferSizeRange;
    AudioValueRange range;
    UInt32 size = sizeof(range);
    err = AudioObjectGetPropertyData(output_device_id_, &property_address, 0, nullptr, &size, &range);
    if (err != noErr)
    {
        LOG(LS_ERROR) << "AudioObjectGetPropertyData failed";
        return false;
    }

    if (range.mMinimum > buf_byte_count)
        buf_byte_count = range.mMinimum;
    else if (range.mMaximum < buf_byte_count)
        buf_byte_count = range.mMaximum;

    property_address.mSelector = kAudioDevicePropertyBufferSize;
    size = sizeof(buf_byte_count);
    err = AudioObjectSetPropertyData(
        output_device_id_, &property_address, 0, nullptr, size, &buf_byte_count);
    if (err != noErr)
    {
        LOG(LS_ERROR) << "AudioObjectSetPropertyData failed";
        return false;
    }

    return true;
}

OSStatus AudioOutputMac::objectListenerProc(AudioObjectID object_id,
                                            UInt32 number_addresses,
                                            const AudioObjectPropertyAddress addresses[],
                                            void* client_data)
{
    AudioOutputMac* self = reinterpret_cast<AudioOutputMac*>(client_data);
    DCHECK(self);

    self->implObjectListenerProc(object_id, number_addresses, addresses);
    // AudioObjectPropertyListenerProc functions are supposed to return 0
    return 0;
}

void AudioOutputMac::implObjectListenerProc(const AudioObjectID object_id,
                                            const UInt32 number_addresses,
                                            const AudioObjectPropertyAddress addresses[])
{
    for (UInt32 i = 0; i < number_addresses; ++i)
    {
        if (addresses[i].mSelector == kAudioHardwarePropertyDevices)
            handleDeviceChange();
        else if (addresses[i].mSelector == kAudioDevicePropertyStreamFormat)
            handleStreamFormatChange(object_id, addresses[i]);
    }
}

void AudioOutputMac::handleDeviceChange()
{
    AudioObjectPropertyAddress property_address =
        { kAudioDevicePropertyDeviceIsAlive, kAudioDevicePropertyScopeOutput, 0 };
    UInt32 device_is_alive = 1;
    UInt32 size = sizeof(UInt32);
    OSStatus err = AudioObjectGetPropertyData(
        output_device_id_, &property_address, 0, nullptr, &size, &device_is_alive);

    if (err == kAudioHardwareBadDeviceError || device_is_alive == 0)
    {
        LOG(LS_ERROR) << "Render device is not alive (probably removed)";
        is_device_alive_ = false;
    }
    else if (err != noErr)
    {
        LOG(LS_ERROR) << "AudioDeviceGetPropertyData failed";
    }
}

void AudioOutputMac::handleStreamFormatChange(const AudioObjectID object_id,
                                              const AudioObjectPropertyAddress property_address)
{
    if (object_id != output_device_id_)
        return;

    // Get the new device format
    AudioStreamBasicDescription stream_format;
    UInt32 size = sizeof(stream_format);
    OSStatus err = AudioObjectGetPropertyData(
        object_id, &property_address, 0, nullptr, &size, &stream_format);
    if (err != noErr)
    {
        LOG(LS_ERROR) << "AudioObjectGetPropertyData failed";
        return;
    }

    if (stream_format.mFormatID != kAudioFormatLinearPCM)
    {
        LOG(LS_ERROR) << "Unacceptable input stream format";
        return;
    }

    if (stream_format.mChannelsPerFrame > kMaxDeviceChannels)
    {
        LOG(LS_ERROR) << "Too many channels on device (mChannelsPerFrame = "
                      << stream_format.mChannelsPerFrame << ")";
        return;
    }

    memcpy(&stream_format_, &stream_format, sizeof(stream_format));
    setDesiredFormat();
}

// static
OSStatus AudioOutputMac::deviceIOProc(AudioDeviceID,
                                      const AudioTimeStamp*,
                                      const AudioBufferList* /* input_data */,
                                      const AudioTimeStamp* /* input_time */,
                                      AudioBufferList* output_data,
                                      const AudioTimeStamp* /* output_time */,
                                      void* client_data)
{
    AudioOutputMac* self = reinterpret_cast<AudioOutputMac*>(client_data);
    DCHECK(self);

    self->implDeviceIOProc(output_data);
    // AudioDeviceIOProc functions are supposed to return 0
    return 0;
}

// static
OSStatus AudioOutputMac::outConverterProc(AudioConverterRef,
                                          UInt32* number_data_packets,
                                          AudioBufferList* data,
                                          AudioStreamPacketDescription**,
                                          void* user_data)
{
    AudioOutputMac* self = reinterpret_cast<AudioOutputMac*>(user_data);
    DCHECK(self);
    return self->implOutConverterProc(number_data_packets, data);
}

void AudioOutputMac::implDeviceIOProc(AudioBufferList* output_data)
{
    // Check if we should close down audio device. Double-checked locking optimization to remove
    // locking overhead.
    if (do_stop_)
    {
        std::scoped_lock lock(lock_);
        if (do_stop_)
        {
            // In the case of a shared device, the single driving ioProc is stopped here.
            AudioDeviceStop(output_device_id_, device_io_proc_id_);
            AudioDeviceDestroyIOProcID(output_device_id_, device_io_proc_id_);

            do_stop_ = false;
            stop_event_.signal();
            return;
        }
    }

    if (!playing_)
    {
        // This can be the case when a shared device is capturing but not rendering. We allow the
        // checks above before returning to avoid a timeout when capturing is stopped.
        return;
    }

    DCHECK(stream_format_.mBytesPerFrame != 0);
    UInt32 size = output_data->mBuffers->mDataByteSize / stream_format_.mBytesPerFrame;

    OSStatus err = AudioConverterFillComplexBuffer(
        converter_, outConverterProc, this, &size, output_data, nullptr);
    if (err != noErr)
    {
        if (err == 1)
        {
            // This is our own error.
            LOG(LS_ERROR) << "AudioConverterFillComplexBuffer failed";
        }
        else
        {
            LOG(LS_ERROR) << "AudioConverterFillComplexBuffer failed";
        }
    }
}

OSStatus AudioOutputMac::implOutConverterProc(UInt32* number_data_packets, AudioBufferList* data)
{
    DCHECK(data->mNumberBuffers == 1);

    PaRingBufferSize num_samples = *number_data_packets * desired_format_.mChannelsPerFrame;

    data->mBuffers->mNumberChannels = desired_format_.mChannelsPerFrame;
    // Always give the converter as much as it wants, zero padding as required.
    data->mBuffers->mDataByteSize = *number_data_packets * desired_format_.mBytesPerPacket;
    data->mBuffers->mData = convert_data_;
    memset(convert_data_, 0, sizeof(convert_data_));

    PaUtil_ReadRingBuffer(pa_render_buffer_.get(), convert_data_, num_samples);

    kern_return_t kern_err = semaphore_signal_all(semaphore_);
    if (kern_err != KERN_SUCCESS)
    {
        LOG(LS_ERROR) << "semaphore_signal_all failed: " << kern_err;
        return 1;
    }

    return 0;
}

} // namespace base
