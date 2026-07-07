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

#include "base/audio/audio_capturer_mac.h"

#include <QTimer>

#import <ScreenCaptureKit/ScreenCaptureKit.h>
#import <CoreMedia/CoreMedia.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <vector>

#include "base/logging.h"
#include "base/audio/audio_silence_detector.h"
#include "proto/desktop_audio.h"

// The project is built without ARC (manual retain/release), so Objective-C objects kept beyond a
// scope are retained explicitly and released in the owner's destructor.

namespace {

const int kSampleRate = 48000;
const int kChannels = 2;

// SCStream produces exact float zeros when nothing plays, and values this small round to zero in
// the int16 conversion anyway.
const int kSilenceThreshold = 0;

// The display list needs time to settle after the stream dies together with its display (the
// filter references one), so the restart is retried a few times before giving up.
const int kMaxRestartAttempts = 5;
const int kRestartDelayMs = 2000;

//--------------------------------------------------------------------------------------------------
qint16 floatToS16(float value)
{
    value = std::clamp(value, -1.0f, 1.0f);
    return static_cast<qint16>(std::lrintf(value * 32767.0f));
}

} // namespace

@class AspiaAudioStreamOutput;

//--------------------------------------------------------------------------------------------------
struct AudioCapturerMacImpl
{
    // For the queued restart notification from the SCStream delegate.
    AudioCapturerMac* capturer = nullptr;

    // Set before the teardown stop so that the delegate can tell a deliberate stop from a
    // spontaneous one and does not schedule a restart on a dying object.
    std::atomic<bool> stopping { false };

    AudioCapturer::PacketCapturedCallback callback;
    AudioSilenceDetector silence_detector { kSilenceThreshold };

    // Reused between samples to avoid a per-callback allocation on the audio path.
    std::vector<qint16> conv_buffer;
    std::vector<quint8> abl_buffer;

    // An unexpected format would repeat for every sample, so it is logged only once.
    bool format_warned = false;

    // ScreenCaptureKit objects.
    SCStream* stream = nil;
    AspiaAudioStreamOutput* output = nil;
    dispatch_queue_t queue = nullptr;
};

//--------------------------------------------------------------------------------------------------
// SCStream output/delegate. Converts each audio sample to interleaved s16 stereo and hands the
// packet to the capturer callback. Runs on the stream's dispatch queue.
@interface AspiaAudioStreamOutput : NSObject <SCStreamOutput, SCStreamDelegate>
{
    AudioCapturerMacImpl* impl_;
}
- (instancetype)initWithImpl:(AudioCapturerMacImpl*)impl;
@end

@implementation AspiaAudioStreamOutput

//--------------------------------------------------------------------------------------------------
- (instancetype)initWithImpl:(AudioCapturerMacImpl*)impl
{
    self = [super init];
    if (self)
        impl_ = impl;
    return self;
}

//--------------------------------------------------------------------------------------------------
- (void)stream:(SCStream*)stream
    didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
                   ofType:(SCStreamOutputType)type
{
    if (type != SCStreamOutputTypeAudio)
        return;

    if (!sampleBuffer || !CMSampleBufferIsValid(sampleBuffer) ||
        !CMSampleBufferDataIsReady(sampleBuffer))
        return;

    CMFormatDescriptionRef format = CMSampleBufferGetFormatDescription(sampleBuffer);
    if (!format)
        return;

    const AudioStreamBasicDescription* desc =
        CMAudioFormatDescriptionGetStreamBasicDescription(format);

    // The stream is configured for 48kHz float32; anything else means the OS ignored the
    // configuration, and converting blindly would produce garbage sound.
    if (!desc || desc->mFormatID != kAudioFormatLinearPCM ||
        !(desc->mFormatFlags & kAudioFormatFlagIsFloat) || desc->mBitsPerChannel != 32 ||
        static_cast<int>(desc->mSampleRate) != kSampleRate)
    {
        if (!impl_->format_warned)
        {
            impl_->format_warned = true;
            LOG(ERROR) << "Unexpected audio format from SCStream";
        }
        return;
    }

    // Two-call pattern: the buffer list size depends on the number of non-interleaved channels,
    // so it is queried first and the (reused) storage is grown to fit.
    size_t abl_size = 0;
    OSStatus status = CMSampleBufferGetAudioBufferListWithRetainedBlockBuffer(
        sampleBuffer, &abl_size, nullptr, 0, nullptr, nullptr, 0, nullptr);
    if (status != noErr || !abl_size)
        return;

    if (impl_->abl_buffer.size() < abl_size)
        impl_->abl_buffer.resize(abl_size);

    AudioBufferList* abl = reinterpret_cast<AudioBufferList*>(impl_->abl_buffer.data());
    CMBlockBufferRef block_buffer = nullptr;

    status = CMSampleBufferGetAudioBufferListWithRetainedBlockBuffer(
        sampleBuffer, nullptr, abl, abl_size, nullptr, nullptr,
        kCMSampleBufferFlag_AudioBufferList_Assure16ByteAlignment, &block_buffer);
    if (status != noErr || !block_buffer)
        return;

    [self processBufferList:abl];

    // The block buffer keeps the audio memory referenced by |abl| alive, so it is released only
    // after the conversion.
    CFRelease(block_buffer);
}

//--------------------------------------------------------------------------------------------------
- (void)processBufferList:(const AudioBufferList*)abl
{
    const float* left = nullptr;
    const float* right = nullptr;
    size_t frames = 0;
    bool interleaved = false;

    if (abl->mNumberBuffers >= 2 && abl->mBuffers[0].mNumberChannels == 1)
    {
        // Non-interleaved stereo, one mono buffer per channel: the usual SCStream layout.
        left = static_cast<const float*>(abl->mBuffers[0].mData);
        right = static_cast<const float*>(abl->mBuffers[1].mData);
        frames = abl->mBuffers[0].mDataByteSize / sizeof(float);
    }
    else if (abl->mNumberBuffers == 1 && abl->mBuffers[0].mNumberChannels == 2)
    {
        left = static_cast<const float*>(abl->mBuffers[0].mData);
        frames = abl->mBuffers[0].mDataByteSize / (kChannels * sizeof(float));
        interleaved = true;
    }
    else if (abl->mNumberBuffers == 1 && abl->mBuffers[0].mNumberChannels == 1)
    {
        // Mono: duplicated into both output channels below.
        left = static_cast<const float*>(abl->mBuffers[0].mData);
        right = left;
        frames = abl->mBuffers[0].mDataByteSize / sizeof(float);
    }
    else
    {
        if (!impl_->format_warned)
        {
            impl_->format_warned = true;
            LOG(ERROR) << "Unexpected audio buffer layout from SCStream";
        }
        return;
    }

    if (!left || !frames)
        return;

    impl_->conv_buffer.resize(frames * kChannels);
    qint16* out = impl_->conv_buffer.data();

    if (interleaved)
    {
        for (size_t i = 0; i < frames * kChannels; ++i)
            out[i] = floatToS16(left[i]);
    }
    else
    {
        for (size_t i = 0; i < frames; ++i)
        {
            out[i * 2] = floatToS16(left[i]);
            out[i * 2 + 1] = floatToS16(right[i]);
        }
    }

    if (impl_->silence_detector.isSilence(out, frames))
        return;

    auto packet = std::make_unique<proto::audio::Packet>();
    packet->add_data(out, frames * kChannels * sizeof(qint16));
    packet->set_encoding(proto::audio::ENCODING_RAW);
    packet->set_sampling_rate(proto::audio::Packet::SAMPLING_RATE_48000);
    packet->set_bytes_per_sample(proto::audio::Packet::BYTES_PER_SAMPLE_2);
    packet->set_channels(proto::audio::Packet::CHANNELS_STEREO);

    impl_->callback(std::move(packet));
}

//--------------------------------------------------------------------------------------------------
- (void)stream:(SCStream*)stream didStopWithError:(NSError*)error
{
    LOG(ERROR) << "Audio SCStream stopped with error: "
               << (error ? error.localizedDescription.UTF8String : "unknown");

    // Teardown stops the stream too; only a spontaneous stop needs the restart path. The queued
    // invocation is delivered on the capturer's thread, so the restart does not run here on the
    // dispatch queue. The slot is named because a member pointer to a private slot is not
    // accessible from this Objective-C method.
    if (impl_->stopping)
        return;

    QMetaObject::invokeMethod(impl_->capturer, "onStreamStopped", Qt::QueuedConnection);
}

@end

namespace {

//--------------------------------------------------------------------------------------------------
// Synchronously fetches the current shareable content. Returns a retained object (caller releases),
// or nil on failure.
SCShareableContent* copyShareableContent()
{
    __block SCShareableContent* result = nil;
    dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);

    [SCShareableContent getShareableContentExcludingDesktopWindows:NO
                                               onScreenWindowsOnly:NO
                                                 completionHandler:^(SCShareableContent* content,
                                                                     NSError* error)
    {
        if (error)
            LOG(ERROR) << "getShareableContent failed: " << error.localizedDescription.UTF8String;
        else
            result = [content retain];

        dispatch_semaphore_signal(semaphore);
    }];

    dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);
    dispatch_release(semaphore);
    return result;
}

} // namespace

//--------------------------------------------------------------------------------------------------
// static
bool AudioCapturer::isSupported()
{
    // Video capture through ScreenCaptureKit works from macOS 12.3, but the audio part appeared
    // only in macOS 13, hence the runtime check instead of a build-time one.
    if (@available(macOS 13.0, *))
        return true;

    return false;
}

//--------------------------------------------------------------------------------------------------
AudioCapturerMac::AudioCapturerMac(QObject* parent)
    : AudioCapturer(parent),
      impl_(std::make_unique<AudioCapturerMacImpl>())
{
    LOG(INFO) << "Ctor";
    impl_->capturer = this;
}

//--------------------------------------------------------------------------------------------------
AudioCapturerMac::~AudioCapturerMac()
{
    LOG(INFO) << "Dtor";

    impl_->stopping = true;
    stopStream();

    [impl_->output release];
    impl_->output = nil;

    if (impl_->queue)
    {
        dispatch_release(impl_->queue);
        impl_->queue = nullptr;
    }
}

//--------------------------------------------------------------------------------------------------
bool AudioCapturerMac::start(const PacketCapturedCallback& callback)
{
    if (@available(macOS 13.0, *))
    {
        impl_->callback = callback;
        impl_->silence_detector.reset(kSampleRate, kChannels);

        impl_->queue = dispatch_queue_create("org.aspia.audio-capture", DISPATCH_QUEUE_SERIAL);
        impl_->output = [[AspiaAudioStreamOutput alloc] initWithImpl:impl_.get()];

        return startStream();
    }

    LOG(ERROR) << "System audio capture requires macOS 13 or newer";
    return false;
}

//--------------------------------------------------------------------------------------------------
void AudioCapturerMac::onStreamStopped()
{
    // Release what is left of the dead stream before creating a new one.
    stopStream();

    if (startStream())
    {
        restart_attempts_ = 0;
        return;
    }

    if (++restart_attempts_ <= kMaxRestartAttempts)
    {
        QTimer::singleShot(kRestartDelayMs, this, &AudioCapturerMac::onStreamStopped);
    }
    else
    {
        // Most likely the Screen Recording permission was revoked; retrying forever would only
        // spam the log.
        LOG(ERROR) << "Failed to restart audio capture, giving up";
    }
}

//--------------------------------------------------------------------------------------------------
bool AudioCapturerMac::startStream()
{
    if (@available(macOS 13.0, *))
    {
        SCShareableContent* content = copyShareableContent();
        if (!content)
            return false;

        if (content.displays.count == 0)
        {
            LOG(ERROR) << "No displays to build a content filter from";
            [content release];
            return false;
        }

        // Audio does not belong to a display, but ScreenCaptureKit accepts only content-based
        // filters. A filter with any display and no excluded windows means "audio of all
        // applications".
        SCContentFilter* filter =
            [[SCContentFilter alloc] initWithDisplay:content.displays[0] excludingWindows:@[]];

        SCStreamConfiguration* config = [[SCStreamConfiguration alloc] init];
        config.capturesAudio = YES;
        // Do not capture what this process plays itself: the client audio played back on the host
        // would otherwise loop into the session.
        config.excludesCurrentProcessAudio = YES;
        config.sampleRate = kSampleRate;
        config.channelCount = kChannels;

        // Video frames are produced regardless of outputs, but no screen output is added, so make
        // the video side of the pipeline as cheap as possible.
        config.width = 2;
        config.height = 2;
        config.minimumFrameInterval = CMTimeMake(1, 1);
        config.showsCursor = NO;

        SCStream* stream = [[SCStream alloc] initWithFilter:filter
                                              configuration:config
                                                   delegate:impl_->output];
        [filter release];
        [config release];
        [content release];

        NSError* output_error = nil;
        if (![stream addStreamOutput:impl_->output
                                type:SCStreamOutputTypeAudio
                  sampleHandlerQueue:impl_->queue
                               error:&output_error])
        {
            LOG(ERROR) << "addStreamOutput failed: "
                       << (output_error ? output_error.localizedDescription.UTF8String : "unknown");
            [stream release];
            return false;
        }

        __block bool started = false;
        dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);

        [stream startCaptureWithCompletionHandler:^(NSError* error)
        {
            if (error)
                LOG(ERROR) << "startCapture failed: " << error.localizedDescription.UTF8String;
            else
                started = true;

            dispatch_semaphore_signal(semaphore);
        }];

        dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);
        dispatch_release(semaphore);

        if (!started)
        {
            [stream release];
            return false;
        }

        impl_->stream = stream;
        LOG(INFO) << "Audio capture started";
        return true;
    }

    return false;
}

//--------------------------------------------------------------------------------------------------
void AudioCapturerMac::stopStream()
{
    if (!impl_->stream)
        return;

    dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);

    [impl_->stream stopCaptureWithCompletionHandler:^(NSError* error)
    {
        if (error)
            LOG(ERROR) << "stopCapture failed: " << error.localizedDescription.UTF8String;

        dispatch_semaphore_signal(semaphore);
    }];

    dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);
    dispatch_release(semaphore);

    [impl_->stream release];
    impl_->stream = nil;
}
