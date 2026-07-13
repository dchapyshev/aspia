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

#include "host/screen_capturer_mac.h"

#include <QThread>

#import <AppKit/AppKit.h>
#import <ScreenCaptureKit/ScreenCaptureKit.h>
#import <AudioToolbox/AudioToolbox.h>
#import <CoreMedia/CoreMedia.h>
#import <CoreVideo/CoreVideo.h>
#import <CoreGraphics/CoreGraphics.h>

#include <atomic>
#include <mutex>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/desktop/frame.h"
#include "base/desktop/frame_aligned.h"
#include "base/desktop/mouse_cursor.h"
#include "base/desktop/region.h"
#include "host/audio_silence_detector.h"
#include "proto/desktop_audio.h"

// The project is built without ARC (manual retain/release), so Objective-C objects kept beyond a
// scope are retained explicitly and released in the owner's destructor.

@class AspiaStreamOutput;

// Lifecycle of the capture stream. STOPPED -> STARTING on a (re)start request; the asynchronous
// completion moves STARTING -> STREAMING or FAILED; an unexpected stop of the live stream also moves
// to FAILED. captureFrame() maps STARTING to a temporary error and FAILED to a permanent one.
enum class StreamState
{
    STOPPED,
    STARTING,
    STREAMING,
    FAILED
};

//--------------------------------------------------------------------------------------------------
struct ScreenCapturerMacImpl
{
    // Back frame. The SCStream delegate (on its dispatch queue) copies the dirty rectangles of each
    // sample into it and accumulates them in |accumulated_region| until captureFrame() on the Qt
    // thread transfers them to the front frame. Both are guarded by |mutex|. The frame is allocated
    // once and reused; it is recreated only when the capture size changes.
    std::mutex mutex;
    std::unique_ptr<Frame> back_frame;
    Region accumulated_region;

    // Set by the CoreGraphics display reconfiguration callback (any thread); consumed on the Qt
    // thread. Covers resolution, position, scale and plug/unplug changes.
    std::atomic<bool> displays_changed { false };

    // The SCStream lifecycle. The stream is created and started asynchronously by AspiaStreamOutput
    // on ScreenCaptureKit's queues while the Qt thread may select another screen or stop capture, so
    // these fields are guarded by |stream_lock|. |start_generation| identifies the newest start
    // request: completion handlers of superseded requests find a different value and discard their
    // stream. |stream_state| is written under the lock and read lock-free by captureFrame().
    std::mutex stream_lock;
    SCStream* stream = nil;
    quint64 start_generation = 0;
    std::atomic<StreamState> stream_state { StreamState::STOPPED };

    // ScreenCaptureKit objects.
    AspiaStreamOutput* output = nil;
    dispatch_queue_t queue = nullptr;

    // System audio captured by the same stream (one stream -> one permission prompt). The delegate
    // converts each audio sample on |audio_queue| (via |audio_converter|) and emits it through
    // |owner|'s sig_audioCaptured; the silence detector and the scratch buffers are per-stream state.
    ScreenCapturerMac* owner = nullptr;
    dispatch_queue_t audio_queue = nullptr;
    AudioConverterRef audio_converter = nullptr;
    AudioSilenceDetector silence_detector { 0 };
    std::vector<qint16> audio_conv_buffer;
    std::vector<quint8> audio_abl_buffer;
    bool audio_format_warned = false;

    // Cached display list, refreshed by updateDisplays().
    struct Display
    {
        CGDirectDisplayID id = 0;
        QRect frame;
        QString title;
        bool primary = false;
    };
    std::vector<Display> displays;
};

namespace {

// System-audio format the shared SCStream is configured for.
const int kAudioSampleRate = 48000;
const int kAudioChannels = 2;

//--------------------------------------------------------------------------------------------------
// Target PCM format for the client: interleaved signed 16-bit stereo at 48 kHz.
AudioStreamBasicDescription outputAudioFormat()
{
    AudioStreamBasicDescription asbd = {};
    asbd.mSampleRate = kAudioSampleRate;
    asbd.mFormatID = kAudioFormatLinearPCM;
    asbd.mFormatFlags = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;
    asbd.mBitsPerChannel = 16;
    asbd.mChannelsPerFrame = kAudioChannels;
    asbd.mFramesPerPacket = 1;
    asbd.mBytesPerFrame = kAudioChannels * sizeof(qint16);
    asbd.mBytesPerPacket = asbd.mBytesPerFrame;
    return asbd;
}

//--------------------------------------------------------------------------------------------------
// Converts one SCStream audio sample buffer to a raw interleaved s16 stereo packet with a CoreAudio
// AudioConverter (float32 -> s16 plus de-interleaving), or nullptr on error or detected silence.
// |converter| is created lazily from the sample's own format and cached; |conv_buffer| / |abl_buffer|
// are scratch storage reused across calls.
std::unique_ptr<proto::audio::Packet> audioSampleToPacket(
    CMSampleBufferRef sample_buffer, AudioConverterRef& converter,
    AudioSilenceDetector& silence_detector, std::vector<qint16>& conv_buffer,
    std::vector<quint8>& abl_buffer, bool& format_warned)
{
    if (!sample_buffer || !CMSampleBufferIsValid(sample_buffer) ||
        !CMSampleBufferDataIsReady(sample_buffer))
        return nullptr;

    CMFormatDescriptionRef format = CMSampleBufferGetFormatDescription(sample_buffer);
    if (!format)
        return nullptr;

    const AudioStreamBasicDescription* input_format =
        CMAudioFormatDescriptionGetStreamBasicDescription(format);
    if (!input_format)
        return nullptr;

    if (!converter)
    {
        const AudioStreamBasicDescription output_format = outputAudioFormat();
        if (AudioConverterNew(input_format, &output_format, &converter) != noErr)
        {
            converter = nullptr;
            if (!format_warned)
            {
                format_warned = true;
                LOG(ERROR) << "AudioConverterNew failed for SCStream audio";
            }
            return nullptr;
        }
    }

    const UInt32 frames = static_cast<UInt32>(CMSampleBufferGetNumSamples(sample_buffer));
    if (!frames)
        return nullptr;

    // Input buffer list in the source (float) format, backed by |abl_buffer| scratch.
    size_t abl_size = 0;
    OSStatus status = CMSampleBufferGetAudioBufferListWithRetainedBlockBuffer(
        sample_buffer, &abl_size, nullptr, 0, nullptr, nullptr, 0, nullptr);
    if (status != noErr || !abl_size)
        return nullptr;

    if (abl_buffer.size() < abl_size)
        abl_buffer.resize(abl_size);

    AudioBufferList* input_abl = reinterpret_cast<AudioBufferList*>(abl_buffer.data());
    CMBlockBufferRef block_buffer = nullptr;

    status = CMSampleBufferGetAudioBufferListWithRetainedBlockBuffer(
        sample_buffer, nullptr, input_abl, abl_size, nullptr, nullptr,
        kCMSampleBufferFlag_AudioBufferList_Assure16ByteAlignment, &block_buffer);
    if (status != noErr || !block_buffer)
        return nullptr;

    // Output: interleaved s16 stereo into |conv_buffer|.
    conv_buffer.resize(static_cast<size_t>(frames) * kAudioChannels);

    AudioBufferList output_abl;
    output_abl.mNumberBuffers = 1;
    output_abl.mBuffers[0].mNumberChannels = kAudioChannels;
    output_abl.mBuffers[0].mDataByteSize =
        static_cast<UInt32>(conv_buffer.size() * sizeof(qint16));
    output_abl.mBuffers[0].mData = conv_buffer.data();

    UInt32 io_frames = frames;
    status = AudioConverterConvertComplexBuffer(converter, io_frames, input_abl, &output_abl);

    // The block buffer keeps the input audio alive; release it only after the conversion.
    CFRelease(block_buffer);

    if (status != noErr)
    {
        if (!format_warned)
        {
            format_warned = true;
            LOG(ERROR) << "AudioConverterConvertComplexBuffer failed:" << status;
        }
        return nullptr;
    }

    qint16* out = conv_buffer.data();
    if (silence_detector.isSilence(out, frames))
        return nullptr;

    auto packet = std::make_unique<proto::audio::Packet>();
    packet->add_data(out, static_cast<size_t>(frames) * kAudioChannels * sizeof(qint16));
    packet->set_encoding(proto::audio::ENCODING_RAW);
    packet->set_sampling_rate(proto::audio::Packet::SAMPLING_RATE_48000);
    packet->set_bytes_per_sample(proto::audio::Packet::BYTES_PER_SAMPLE_2);
    packet->set_channels(proto::audio::Packet::CHANNELS_STEREO);
    return packet;
}

} // namespace

//--------------------------------------------------------------------------------------------------
// SCStream output/delegate. Copies each screen sample into the shared pending frame and owns the
// asynchronous stream start chain (getShareableContent -> create SCStream -> startCapture), which
// runs on ScreenCaptureKit's queues. |lock_| guards |impl_| against the capturer's destruction:
// every callback holds it for its whole body, and invalidate() (called from the capturer's dtor)
// nulls the pointer so late callbacks become no-ops.
@interface AspiaStreamOutput : NSObject <SCStreamOutput, SCStreamDelegate>
{
    ScreenCapturerMacImpl* impl_;
    std::mutex lock_;
}
- (instancetype)initWithImpl:(ScreenCapturerMacImpl*)impl;
- (void)invalidate;
- (void)startStreamGeneration:(quint64)generation displayId:(CGDirectDisplayID)displayId;
@end

@implementation AspiaStreamOutput

//--------------------------------------------------------------------------------------------------
- (instancetype)initWithImpl:(ScreenCapturerMacImpl*)impl
{
    self = [super init];
    if (self)
        impl_ = impl;
    return self;
}

//--------------------------------------------------------------------------------------------------
- (void)invalidate
{
    // After this no callback touches the capturer's state. An in-flight callback finishes first:
    // each one holds |lock_| for its whole body, so this call blocks until it is done.
    std::lock_guard<std::mutex> lock(lock_);
    impl_ = nullptr;
}

//--------------------------------------------------------------------------------------------------
- (void)startStreamGeneration:(quint64)generation displayId:(CGDirectDisplayID)displayId
{
    // Entirely asynchronous (as WebRTC does): fetching the shareable content can stall while a
    // session transition settles, so the capture thread never waits on it. The chain continues in
    // createStreamWithContent and the capturer reports a temporary error until the start completes.
    [SCShareableContent getShareableContentExcludingDesktopWindows:NO
                                               onScreenWindowsOnly:NO
                                                 completionHandler:^(SCShareableContent* content,
                                                                     NSError* error)
    {
        if (error)
        {
            LOG(ERROR) << "getShareableContent failed: " << error.localizedDescription.UTF8String;
            [self markStartFailed:generation];
            return;
        }

        [self createStreamWithContent:content generation:generation displayId:displayId];
    }];
}

//--------------------------------------------------------------------------------------------------
// Runs on ScreenCaptureKit's completion queue. Builds and starts the SCStream for |displayId|.
- (void)createStreamWithContent:(SCShareableContent*)content
                     generation:(quint64)generation
                      displayId:(CGDirectDisplayID)displayId
{
    std::lock_guard<std::mutex> lock(lock_);
    if (!impl_)
        return;

    {
        std::lock_guard<std::mutex> stream_lock(impl_->stream_lock);
        if (generation != impl_->start_generation)
            return; // Superseded by a newer start or a stop.
    }

    SCDisplay* target = nil;
    for (SCDisplay* display in content.displays)
    {
        if (display.displayID == displayId)
        {
            target = display;
            break;
        }
    }

    if (!target)
    {
        LOG(ERROR) << "Display not found for capture:" << displayId;
        [self markStartFailedLocked:generation];
        return;
    }

    SCContentFilter* filter =
        [[SCContentFilter alloc] initWithDisplay:target excludingWindows:@[]];

    SCStreamConfiguration* config = [[SCStreamConfiguration alloc] init];
    config.width = static_cast<size_t>(target.frame.size.width);
    config.height = static_cast<size_t>(target.frame.size.height);
    config.pixelFormat = kCVPixelFormatType_32BGRA;
    config.showsCursor = NO;
    config.queueDepth = 5;

    // Without this ScreenCaptureKit delivers at the display's native refresh (60/120 Hz), and the
    // delegate copies each delivered frame even though the agent consumes at most 30 fps - wasting
    // memory bandwidth that starves the encoder. Cap delivery at the remote-desktop maximum.
    config.minimumFrameInterval = CMTimeMake(1, 30);

    // Capture system audio on this same stream so screen and audio share a single permission prompt.
    // Skip what this process plays itself, or the client audio played back on the host would loop
    // back into the session.
    config.capturesAudio = YES;
    config.excludesCurrentProcessAudio = YES;
    config.sampleRate = kAudioSampleRate;
    config.channelCount = kAudioChannels;

    SCStream* stream = [[SCStream alloc] initWithFilter:filter
                                          configuration:config
                                               delegate:self];
    [filter release];
    [config release];

    NSError* output_error = nil;
    if (![stream addStreamOutput:self
                            type:SCStreamOutputTypeScreen
              sampleHandlerQueue:impl_->queue
                           error:&output_error])
    {
        LOG(ERROR) << "addStreamOutput failed: "
                   << (output_error ? output_error.localizedDescription.UTF8String : "unknown");
        [stream release];
        [self markStartFailedLocked:generation];
        return;
    }

    NSError* audio_error = nil;
    if (![stream addStreamOutput:self
                            type:SCStreamOutputTypeAudio
              sampleHandlerQueue:impl_->audio_queue
                           error:&audio_error])
    {
        // Audio is best-effort: a failure here (e.g. macOS < 13) must not prevent screen capture.
        LOG(ERROR) << "addStreamOutput (audio) failed:"
                   << (audio_error ? audio_error.localizedDescription.UTF8String : "unknown");
    }

    [stream startCaptureWithCompletionHandler:^(NSError* error)
    {
        [self streamDidStart:stream generation:generation error:error];
    }];

    std::lock_guard<std::mutex> stream_lock(impl_->stream_lock);
    if (generation != impl_->start_generation)
    {
        // Superseded while starting: this stream is not the current one anymore, discard it.
        [stream stopCaptureWithCompletionHandler:^(NSError* /* error */) { [stream release]; }];
        return;
    }

    // Ownership passes to the impl; stopStream() releases it.
    impl_->stream = stream;
}

//--------------------------------------------------------------------------------------------------
- (void)markStartFailed:(quint64)generation
{
    std::lock_guard<std::mutex> lock(lock_);
    if (!impl_)
        return;

    [self markStartFailedLocked:generation];
}

//--------------------------------------------------------------------------------------------------
// Requires |lock_| held with a valid impl.
- (void)markStartFailedLocked:(quint64)generation
{
    std::lock_guard<std::mutex> stream_lock(impl_->stream_lock);

    // Only the current generation may fail the state; a stale start must not clobber a newer one.
    if (generation == impl_->start_generation)
        impl_->stream_state.store(StreamState::FAILED, std::memory_order_relaxed);
}

//--------------------------------------------------------------------------------------------------
- (void)streamDidStart:(SCStream*)stream generation:(quint64)generation error:(NSError*)error
{
    std::lock_guard<std::mutex> lock(lock_);
    if (!impl_)
        return;

    std::lock_guard<std::mutex> stream_lock(impl_->stream_lock);
    if (generation != impl_->start_generation)
        return; // Superseded; the stream has already been discarded.

    if (error)
    {
        LOG(ERROR) << "startCapture failed: " << error.localizedDescription.UTF8String;

        if (impl_->stream == stream)
        {
            [impl_->stream release];
            impl_->stream = nil;
        }

        impl_->stream_state.store(StreamState::FAILED, std::memory_order_relaxed);
        return;
    }

    // Only the pending start may move the state to STREAMING. During session transitions the stream
    // can die (didStopWithError sets FAILED) before this "success" completion is delivered; a FAILED
    // state must survive it, or a dead stream would be considered live and never recreated.
    if (impl_->stream != stream ||
        impl_->stream_state.load(std::memory_order_relaxed) != StreamState::STARTING)
    {
        LOG(WARNING) << "Late start completion ignored (stream already stopped or replaced)";
        return;
    }

    LOG(INFO) << "Capture stream started";
    impl_->stream_state.store(StreamState::STREAMING, std::memory_order_relaxed);
}

//--------------------------------------------------------------------------------------------------
- (void)stream:(SCStream*)stream
    didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
                   ofType:(SCStreamOutputType)type
{
    std::lock_guard<std::mutex> impl_lock(lock_);
    if (!impl_)
        return;

    if (type == SCStreamOutputTypeAudio)
    {
        std::unique_ptr<proto::audio::Packet> packet = audioSampleToPacket(
            sampleBuffer, impl_->audio_converter, impl_->silence_detector,
            impl_->audio_conv_buffer, impl_->audio_abl_buffer, impl_->audio_format_warned);
        if (packet)
        {
            // Emitted from the audio dispatch queue; the connected worker receives it queued.
            emit impl_->owner->sig_audioCaptured(std::shared_ptr<proto::audio::Packet>(std::move(packet)));
        }
        return;
    }

    if (type != SCStreamOutputTypeScreen)
        return;

    if (!sampleBuffer || !CMSampleBufferIsValid(sampleBuffer))
        return;

    @autoreleasepool
    {
        // Only "complete" frames carry pixels; idle/blank/suspended status updates do not.
        NSArray* attachments =
            (NSArray*)CMSampleBufferGetSampleAttachmentsArray(sampleBuffer, false);
        if (attachments.count == 0)
            return;

        NSDictionary* info = attachments[0];
        NSNumber* status = info[SCStreamFrameInfoStatus];
        if (!status || (SCFrameStatus)status.integerValue != SCFrameStatusComplete)
            return;

        CVImageBufferRef image = CMSampleBufferGetImageBuffer(sampleBuffer);
        if (!image)
            return;

        if (CVPixelBufferLockBaseAddress(image, kCVPixelBufferLock_ReadOnly) != kCVReturnSuccess)
            return;

        const size_t width = CVPixelBufferGetWidth(image);
        const size_t height = CVPixelBufferGetHeight(image);
        const int src_stride = static_cast<int>(CVPixelBufferGetBytesPerRow(image));
        const auto* src = static_cast<const quint8*>(CVPixelBufferGetBaseAddress(image));

        if (src && width > 0 && height > 0)
        {
            const QRect frame_rect(0, 0, static_cast<int>(width), static_cast<int>(height));

            // The dirty rectangles reported by ScreenCaptureKit; only these pixels are copied.
            // Fall back to the whole frame when the sample carries no dirty info.
            Region dirty_region;
            NSArray* dirty_rects = info[SCStreamFrameInfoDirtyRects];
            for (id value in dirty_rects)
            {
                CGRect rect = CGRectZero;
                if (CGRectMakeWithDictionaryRepresentation((CFDictionaryRef)value, &rect))
                {
                    dirty_region += QRect(static_cast<int>(rect.origin.x),
                                          static_cast<int>(rect.origin.y),
                                          static_cast<int>(rect.size.width),
                                          static_cast<int>(rect.size.height));
                }
            }

            if (dirty_region.isEmpty())
                dirty_region += frame_rect;
            else
                dirty_region.intersect(frame_rect);

            std::lock_guard<std::mutex> lock(impl_->mutex);

            // The back frame is reused between samples; it is recreated (with a full-frame copy)
            // only when the capture size changes.
            if (!impl_->back_frame || impl_->back_frame->size() != frame_rect.size())
            {
                impl_->back_frame = FrameAligned::create(frame_rect.size(), 32);
                if (impl_->back_frame)
                {
                    dirty_region.clear();
                    dirty_region += frame_rect;
                }
            }

            if (impl_->back_frame)
            {
                for (const QRect& rect : dirty_region)
                {
                    const quint8* rect_src =
                        src + rect.y() * src_stride + rect.x() * Frame::kBytesPerPixel;
                    impl_->back_frame->copyPixelsFrom(rect_src, src_stride, rect);
                }

                impl_->accumulated_region += dirty_region;
            }
        }

        CVPixelBufferUnlockBaseAddress(image, kCVPixelBufferLock_ReadOnly);
    }
}

//--------------------------------------------------------------------------------------------------
- (void)stream:(SCStream*)stream didStopWithError:(NSError*)error
{
    std::lock_guard<std::mutex> lock(lock_);
    if (!impl_)
        return;

    LOG(ERROR) << "SCStream stopped with error: "
               << (error ? error.localizedDescription.UTF8String : "unknown");

    std::lock_guard<std::mutex> stream_lock(impl_->stream_lock);

    // Only the live stream's death matters; a superseded stream dying during teardown is expected.
    if (impl_->stream == stream)
        impl_->stream_state.store(StreamState::FAILED, std::memory_order_relaxed);
}

@end

namespace {

// How long an asynchronous stream start may stay pending before captureFrame() reports a permanent
// error. Generous: right after a login/logout transition ScreenCaptureKit can take seconds to
// respond while the WindowServer settles.
const qint64 kStreamStartTimeoutMs = 10000;

//--------------------------------------------------------------------------------------------------
QRect cgRectToQRect(CGRect rect)
{
    return QRect(static_cast<int>(rect.origin.x),
                 static_cast<int>(rect.origin.y),
                 static_cast<int>(rect.size.width),
                 static_cast<int>(rect.size.height));
}

//--------------------------------------------------------------------------------------------------
// Invoked by CoreGraphics whenever the display configuration changes (resolution, position, scale,
// plug/unplug, mirroring). Only the "after" notification matters; the "begin" one precedes the
// actual change.
void displayReconfigurationCallback(CGDirectDisplayID /* display */,
                                    CGDisplayChangeSummaryFlags flags,
                                    void* user_info)
{
    if (flags & kCGDisplayBeginConfigurationFlag)
        return;

    static_cast<ScreenCapturerMacImpl*>(user_info)->displays_changed = true;
}

} // namespace

//--------------------------------------------------------------------------------------------------
ScreenCapturerMac::ScreenCapturerMac(QObject* parent)
    : ScreenCapturer(ScreenCapturer::Type::MACOSX, parent),
      impl_(std::make_unique<ScreenCapturerMacImpl>())
{
    LOG(INFO) << "Ctor";

    impl_->owner = this;
    impl_->queue = dispatch_queue_create("org.aspia.screencapture", DISPATCH_QUEUE_SERIAL);
    impl_->audio_queue = dispatch_queue_create("org.aspia.screencapture.audio", DISPATCH_QUEUE_SERIAL);
    impl_->output = [[AspiaStreamOutput alloc] initWithImpl:impl_.get()];
    impl_->silence_detector.reset(kAudioSampleRate, kAudioChannels);

    CGDisplayRegisterReconfigurationCallback(displayReconfigurationCallback, impl_.get());
}

//--------------------------------------------------------------------------------------------------
ScreenCapturerMac::~ScreenCapturerMac()
{
    LOG(INFO) << "Dtor";

    // Remove the callback first: it references impl_, which is destroyed below.
    CGDisplayRemoveReconfigurationCallback(displayReconfigurationCallback, impl_.get());

    // Detach the output/delegate from the capturer before stopping: sample callbacks and start/stop
    // completions may still be in flight on ScreenCaptureKit's queues, and after this they are
    // no-ops. In-flight blocks hold their own references to the output, so releasing it below is
    // safe - it is deallocated when the last of them finishes.
    [impl_->output invalidate];

    stopStream();

    [impl_->output release];
    impl_->output = nil;

    if (impl_->queue)
    {
        dispatch_release(impl_->queue);
        impl_->queue = nullptr;
    }

    if (impl_->audio_queue)
    {
        dispatch_release(impl_->audio_queue);
        impl_->audio_queue = nullptr;
    }

    if (impl_->audio_converter)
    {
        AudioConverterDispose(impl_->audio_converter);
        impl_->audio_converter = nullptr;
    }
}

//--------------------------------------------------------------------------------------------------
// static
ScreenCapturerMac* ScreenCapturerMac::create(QObject* parent)
{
    std::unique_ptr<ScreenCapturerMac> self(new ScreenCapturerMac(parent));

    if (!self->updateDisplays())
    {
        LOG(ERROR) << "No displays available for capture";
        return nullptr;
    }

    // Default to the primary display; the agent may switch later via selectScreen().
    self->current_screen_id_ = static_cast<ScreenId>(CGMainDisplayID());
    for (const auto& display : self->impl_->displays)
    {
        if (display.primary)
        {
            self->current_screen_id_ = static_cast<ScreenId>(display.id);
            self->current_screen_rect_ = display.frame;
            break;
        }
    }

    if (!self->startStream())
    {
        LOG(ERROR) << "Failed to start capture stream";
        return nullptr;
    }

    return self.release();
}

//--------------------------------------------------------------------------------------------------
// static
bool ScreenCapturerMac::waitForDisplays()
{
    static const int kRetryCount = 20;
    static const int kRetryDelayMs = 250;

    for (int i = 0; i < kRetryCount; ++i)
    {
        CGDirectDisplayID display;
        quint32 count = 0;
        if (CGGetOnlineDisplayList(1, &display, &count) == kCGErrorSuccess && count > 0)
            return true;

        QThread::msleep(kRetryDelayMs);
    }

    return false;
}

//--------------------------------------------------------------------------------------------------
int ScreenCapturerMac::screenCount()
{
    // The agent polls this every capture cycle to detect plugged/unplugged displays, so refresh the
    // cache when the configuration changed. The flag itself stays set: captureFrame() consumes it
    // and restarts the stream if the captured display was affected.
    if (impl_->displays_changed.load(std::memory_order_relaxed))
        updateDisplays();

    return static_cast<int>(impl_->displays.size());
}

//--------------------------------------------------------------------------------------------------
bool ScreenCapturerMac::screenList(ScreenList* screens)
{
    if (impl_->displays_changed.load(std::memory_order_relaxed))
        updateDisplays();

    if (impl_->displays.empty())
        return false;

    screens->screens.clear();
    screens->resolutions.clear();

    for (const auto& display : impl_->displays)
    {
        Screen screen;
        screen.id = static_cast<ScreenId>(display.id);
        screen.title = display.title;
        screen.position = display.frame.topLeft();
        screen.resolution = display.frame.size();
        screen.dpi = QPoint(static_cast<int>(Frame::kStandardDPI),
                            static_cast<int>(Frame::kStandardDPI));
        screen.is_primary = display.primary;

        screens->screens.append(screen);
        screens->resolutions.append(display.frame.size());
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool ScreenCapturerMac::selectScreen(ScreenId screen_id)
{
    if (!updateDisplays())
        return false;

    // Validate that the requested display exists.
    const ScreenCapturerMacImpl::Display* target = nullptr;
    for (const auto& display : impl_->displays)
    {
        if (static_cast<ScreenId>(display.id) == screen_id)
        {
            target = &display;
            break;
        }
    }

    if (!target)
    {
        LOG(ERROR) << "Requested display not found:" << screen_id;
        return false;
    }

    const StreamState state = impl_->stream_state.load(std::memory_order_relaxed);
    if (screen_id == current_screen_id_ &&
        (state == StreamState::STARTING || state == StreamState::STREAMING))
        return true;

    current_screen_id_ = screen_id;
    current_screen_rect_ = target->frame;

    stopStream();
    reset();

    return startStream();
}

//--------------------------------------------------------------------------------------------------
ScreenCapturer::ScreenId ScreenCapturerMac::currentScreen() const
{
    return current_screen_id_;
}

//--------------------------------------------------------------------------------------------------
const Frame* ScreenCapturerMac::captureFrame(Error* error)
{
    const StreamState state = impl_->stream_state.load(std::memory_order_relaxed);
    if (state == StreamState::STARTING)
    {
        // The asynchronous start has not completed yet: report a temporary error while it is
        // pending. If the WindowServer stalls it beyond any reasonable time (login/logout
        // transitions), report a permanent one so the agent recreates the capturer against the
        // settled session.
        if (start_deadline_.hasExpired())
        {
            LOG(ERROR) << "Capture stream start timed out";
            *error = Error::PERMANENT;
            return nullptr;
        }

        *error = Error::TEMPORARY;
        return nullptr;
    }

    if (state != StreamState::STREAMING)
    {
        // The start failed or the live stream died on its own (e.g. the captured display was
        // disconnected). Report a permanent error so the agent recreates the capturer against the
        // current configuration.
        LOG(ERROR) << "Capture stream is not running";
        *error = Error::PERMANENT;
        return nullptr;
    }

    if (impl_->displays_changed.exchange(false, std::memory_order_relaxed))
    {
        if (!handleDisplayChange())
        {
            *error = Error::PERMANENT;
            return nullptr;
        }
    }

    std::lock_guard<std::mutex> lock(impl_->mutex);

    if (!impl_->back_frame)
    {
        // The stream has not produced its first frame yet.
        *error = Error::TEMPORARY;
        return nullptr;
    }

    // The front frame is reused between calls; it is recreated (with a full-frame transfer) only
    // when the capture size changes.
    if (!front_frame_ || front_frame_->size() != impl_->back_frame->size())
    {
        front_frame_ = FrameAligned::create(impl_->back_frame->size(), 32);
        if (!front_frame_)
        {
            *error = Error::TEMPORARY;
            return nullptr;
        }

        front_frame_->setCapturerType(static_cast<quint32>(type()));

        impl_->accumulated_region.clear();
        impl_->accumulated_region += QRect(QPoint(0, 0), impl_->back_frame->size());
    }

    // Transfer only the pixels that changed since the previous call.
    for (const QRect& rect : std::as_const(impl_->accumulated_region))
        front_frame_->copyPixelsFrom(*impl_->back_frame, rect.topLeft(), rect);

    Region* updated_region = front_frame_->updatedRegion();
    updated_region->clear();
    updated_region->swap(impl_->accumulated_region);

    front_frame_->setTopLeft(current_screen_rect_.topLeft());

    *error = Error::SUCCEEDED;
    return front_frame_.get();
}

//--------------------------------------------------------------------------------------------------
const MouseCursor* ScreenCapturerMac::captureCursor()
{
    @autoreleasepool
    {
        // The cursor currently shown on screen, whichever application set it.
        NSCursor* cursor = [NSCursor currentSystemCursor];
        if (!cursor)
            return nullptr;

        NSImage* ns_image = cursor.image;
        if (!ns_image)
            return nullptr;

        // Returned image is owned by the NSImage; it must not be released here.
        CGImageRef cg_image = [ns_image CGImageForProposedRect:nullptr context:nil hints:nil];
        if (!cg_image)
            return nullptr;

        // The captured video is at the display's logical (point) resolution, so the cursor must be
        // rendered at its logical size too. On a Retina display the source CGImage is at pixel
        // resolution (2x/3x); drawing it into a point-sized context scales it down so the cursor
        // matches the video scale instead of appearing oversized on the client (which sizes the
        // cursor in the frame's coordinate space and does not use its reported DPI).
        const NSSize size_points = ns_image.size;
        const size_t width = static_cast<size_t>(qRound(size_points.width));
        const size_t height = static_cast<size_t>(qRound(size_points.height));
        if (!width || !height)
            return nullptr;

        // Render the cursor into the BGRA layout MouseCursor expects, regardless of the source
        // image representation.
        QByteArray image_data;
        image_data.resize(static_cast<qsizetype>(width * height * MouseCursor::kBytesPerPixel));
        image_data.fill(0);

        CGColorSpaceRef color_space = CGColorSpaceCreateDeviceRGB();
        if (!color_space)
            return nullptr;

        CGContextRef context = CGBitmapContextCreate(
            image_data.data(), width, height, 8, width * MouseCursor::kBytesPerPixel, color_space,
            static_cast<uint32_t>(kCGImageAlphaPremultipliedFirst) |
                static_cast<uint32_t>(kCGBitmapByteOrder32Little));
        CGColorSpaceRelease(color_space);
        if (!context)
            return nullptr;

        // Scale the (possibly Retina-resolution) source image down to the logical point size.
        CGContextSetInterpolationQuality(context, kCGInterpolationHigh);
        CGContextDrawImage(context, CGRectMake(0, 0, width, height), cg_image);
        CGContextRelease(context);

        // hotSpot is reported in points, matching the point-sized image rendered above.
        const NSPoint ns_hotspot = cursor.hotSpot;
        const QPoint hotspot(qRound(ns_hotspot.x), qRound(ns_hotspot.y));

        const QPoint dpi(MouseCursor::kDefaultDpiX, MouseCursor::kDefaultDpiY);

        auto mouse_cursor = std::make_unique<MouseCursor>(
            std::move(image_data),
            QSize(static_cast<int>(width), static_cast<int>(height)),
            hotspot, dpi);

        // Hand the cursor out only when its shape changed since the previous call.
        if (mouse_cursor_ && mouse_cursor_->equals(*mouse_cursor))
            return nullptr;

        mouse_cursor_ = std::move(mouse_cursor);
        return mouse_cursor_.get();
    }
}

//--------------------------------------------------------------------------------------------------
QPoint ScreenCapturerMac::cursorPosition()
{
    CGEventRef event = CGEventCreate(nullptr);
    if (!event)
        return QPoint(0, 0);

    const CGPoint position = CGEventGetLocation(event);
    CFRelease(event);

    // Relative to the captured display's top-left, matching the other platforms.
    return QPoint(static_cast<int>(position.x), static_cast<int>(position.y)) -
           current_screen_rect_.topLeft();
}

//--------------------------------------------------------------------------------------------------
void ScreenCapturerMac::resetCursorCache()
{
    // Clear the remembered shape so the next captureCursor() treats the current cursor as new and
    // emits it even if it has not changed.
    mouse_cursor_.reset();
}

//--------------------------------------------------------------------------------------------------
const QRect& ScreenCapturerMac::desktopRect() const
{
    return desktop_rect_;
}

//--------------------------------------------------------------------------------------------------
const QRect& ScreenCapturerMac::currentScreenRect() const
{
    return current_screen_rect_;
}

//--------------------------------------------------------------------------------------------------
void ScreenCapturerMac::reset()
{
    front_frame_.reset();

    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->back_frame.reset();
    impl_->accumulated_region.clear();
}

//--------------------------------------------------------------------------------------------------
bool ScreenCapturerMac::updateDisplays()
{
    // Enumerated with CoreGraphics rather than SCShareableContent: these calls are synchronous but
    // never block on the WindowServer readiness (RustDesk and WebRTC enumerate the same way), while
    // the shareable-content fetch can stall during session transitions. ScreenCaptureKit is used
    // only where it is unavoidable - to build the capture stream itself.
    CGDirectDisplayID ids[16];
    quint32 count = 0;
    if (CGGetActiveDisplayList(16, ids, &count) != kCGErrorSuccess || count == 0)
    {
        LOG(ERROR) << "CGGetActiveDisplayList failed or no displays";
        return false;
    }

    const CGDirectDisplayID main_display = CGMainDisplayID();

    impl_->displays.clear();
    QRect desktop;

    for (quint32 i = 0; i < count; ++i)
    {
        ScreenCapturerMacImpl::Display info;
        info.id = ids[i];
        info.frame = cgRectToQRect(CGDisplayBounds(ids[i]));
        info.primary = (ids[i] == main_display);
        info.title = QString("Display %1").arg(ids[i]);

        impl_->displays.push_back(info);
        desktop = desktop.united(info.frame);
    }

    desktop_rect_ = desktop;
    return true;
}

//--------------------------------------------------------------------------------------------------
bool ScreenCapturerMac::handleDisplayChange()
{
    LOG(INFO) << "Display configuration changed";

    if (!updateDisplays())
        return false;

    const ScreenCapturerMacImpl::Display* target = nullptr;
    for (const auto& display : impl_->displays)
    {
        if (static_cast<ScreenId>(display.id) == current_screen_id_)
        {
            target = &display;
            break;
        }
    }

    if (!target)
    {
        LOG(INFO) << "Captured display disconnected:" << current_screen_id_;
        return false;
    }

    if (target->frame != current_screen_rect_)
    {
        // Resolution or position of the captured display changed. Restart the stream so its
        // configured dimensions match; otherwise ScreenCaptureKit keeps scaling the content into
        // the stale size and the reported coordinates go out of sync.
        LOG(INFO) << "Captured display rect changed from" << current_screen_rect_
                  << "to" << target->frame;

        stopStream();
        reset();
        return startStream();
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool ScreenCapturerMac::startStream()
{
    if (current_screen_id_ == kInvalidScreenId)
        return false;

    quint64 generation = 0;
    {
        std::lock_guard<std::mutex> lock(impl_->stream_lock);
        generation = ++impl_->start_generation;
        impl_->stream_state.store(StreamState::STARTING, std::memory_order_relaxed);
    }

    start_deadline_.setRemainingTime(kStreamStartTimeoutMs);

    // The shareable-content fetch, the stream creation and the capture start all happen
    // asynchronously on ScreenCaptureKit's queues (as WebRTC does): any of them can stall while a
    // session transition settles, so nothing here may block the capture thread. captureFrame()
    // reports a temporary error until the start completes.
    [impl_->output startStreamGeneration:generation
                               displayId:static_cast<CGDirectDisplayID>(current_screen_id_)];
    return true;
}

//--------------------------------------------------------------------------------------------------
void ScreenCapturerMac::stopStream()
{
    SCStream* stream = nil;
    {
        std::lock_guard<std::mutex> lock(impl_->stream_lock);

        // Invalidates any start still in flight: its completion finds a newer generation and
        // discards its stream.
        ++impl_->start_generation;
        impl_->stream_state.store(StreamState::STOPPED, std::memory_order_relaxed);

        stream = impl_->stream;
        impl_->stream = nil;
    }

    if (!stream)
        return;

    // Asynchronous: never block on the WindowServer. The copied block keeps |stream| and |output|
    // (which the stream may still call into) alive until the stop completes, then the last stream
    // reference is dropped.
    AspiaStreamOutput* output = impl_->output;
    [stream stopCaptureWithCompletionHandler:^(NSError* error)
    {
        if (error)
            LOG(ERROR) << "stopCapture failed: " << error.localizedDescription.UTF8String;

        (void)output;
        [stream release];
    }];
}
