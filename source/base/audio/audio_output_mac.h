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

#ifndef BASE_AUDIO_AUDIO_OUTPUT_MAC_H
#define BASE_AUDIO_AUDIO_OUTPUT_MAC_H

#include "base/audio/audio_output.h"

#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

#include <AudioToolbox/AudioConverter.h>
#include <CoreAudio/CoreAudio.h>
#include <mach/semaphore.h>

struct PaUtilRingBuffer;

namespace base {

class SimpleThread;

class AudioOutputMac final : public AudioOutput
{
public:
    explicit AudioOutputMac(const NeedMoreDataCB& need_more_data_cb);
    ~AudioOutputMac();

    // AudioOutput implementation.
    bool start() final;
    bool stop() final;

private:
    bool initDevice();
    void terminate();
    bool initPlayout();
    bool setDesiredFormat();
    void threadRun();

    static OSStatus objectListenerProc(AudioObjectID object_id,
                                       UInt32 number_addresses,
                                       const AudioObjectPropertyAddress addresses[],
                                       void* client_data);
    void implObjectListenerProc(AudioObjectID object_id,
                                UInt32 number_addresses,
                                const AudioObjectPropertyAddress addresses[]);
    void handleDeviceChange();
    void handleStreamFormatChange(AudioObjectID object_id, AudioObjectPropertyAddress property_address);

    static OSStatus deviceIOProc(AudioDeviceID device,
                                 const AudioTimeStamp* now,
                                 const AudioBufferList* input_data,
                                 const AudioTimeStamp* input_time,
                                 AudioBufferList* output_data,
                                 const AudioTimeStamp* output_time,
                                 void* client_data);
    static OSStatus outConverterProc(AudioConverterRef audio_converter,
                                     UInt32* number_data_packets,
                                     AudioBufferList* data,
                                     AudioStreamPacketDescription** data_packet_description,
                                     void* user_data);
    void implDeviceIOProc(AudioBufferList* output_data);
    OSStatus implOutConverterProc(UInt32* number_data_packets, AudioBufferList* data);

    static const uint32_t kMaxDeviceChannels = 64;
    static const uint32_t kSamplesPerChannel10ms = kSampleRate * 10 / 1000;
    static const int kBlocksIO = 2;
    static const int kBuffersOut = 3;  // Must be at least kBlocksIO.
    static const uint32_t kTimerPeriodMs = 2 * 10 * kBlocksIO * 1000000;
    static const uint32_t kPlayBufferSizeInSamples = kSamplesPerChannel10ms * kChannels * kBuffersOut;

    std::mutex lock_;
    std::condition_variable stop_event_;
    std::unique_ptr<std::thread> worker_thread_;

    AudioDeviceID output_device_id_ = kAudioObjectUnknown;
    AudioDeviceIOProcID device_io_proc_id_;
    std::atomic_bool is_device_alive_ { true };

    bool device_initialized_ = false;
    bool playout_initialized_ = false;
    bool do_stop_ = false;
    bool playing_ = false;

    AudioStreamBasicDescription stream_format_;
    AudioStreamBasicDescription desired_format_;

    AudioConverterRef converter_;
    SInt16 convert_data_[kPlayBufferSizeInSamples];

    std::vector<SInt16> render_buffer_data_;
    std::unique_ptr<PaUtilRingBuffer> pa_render_buffer_;
    int32_t render_delay_offset_samples_ = 0;
    semaphore_t semaphore_;

    Q_DISABLE_COPY(AudioOutputMac)
};

} // namespace base

#endif // BASE_AUDIO_AUDIO_OUTPUT_MAC_H
