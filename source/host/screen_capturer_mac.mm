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

#import <AppKit/AppKit.h>
#import <ScreenCaptureKit/ScreenCaptureKit.h>
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

// The project is built without ARC (manual retain/release), so Objective-C objects kept beyond a
// scope are retained explicitly and released in the owner's destructor.

@class AspiaStreamOutput;

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

    // Set by the SCStream delegate when the stream stops on its own (e.g. the captured display was
    // disconnected). captureFrame() then reports a permanent error so the capturer is recreated.
    std::atomic<bool> stream_stopped { false };

    // ScreenCaptureKit objects.
    SCStream* stream = nil;
    AspiaStreamOutput* output = nil;
    dispatch_queue_t queue = nullptr;

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

//--------------------------------------------------------------------------------------------------
// SCStream output/delegate. Copies each screen sample into the shared pending frame.
@interface AspiaStreamOutput : NSObject <SCStreamOutput, SCStreamDelegate>
{
    ScreenCapturerMacImpl* impl_;
}
- (instancetype)initWithImpl:(ScreenCapturerMacImpl*)impl;
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
- (void)stream:(SCStream*)stream
    didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
                   ofType:(SCStreamOutputType)type
{
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
    LOG(ERROR) << "SCStream stopped with error: "
               << (error ? error.localizedDescription.UTF8String : "unknown");

    impl_->stream_stopped = true;
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

    impl_->queue = dispatch_queue_create("org.aspia.screencapture", DISPATCH_QUEUE_SERIAL);
    impl_->output = [[AspiaStreamOutput alloc] initWithImpl:impl_.get()];

    CGDisplayRegisterReconfigurationCallback(displayReconfigurationCallback, impl_.get());
}

//--------------------------------------------------------------------------------------------------
ScreenCapturerMac::~ScreenCapturerMac()
{
    LOG(INFO) << "Dtor";

    // Remove the callback first: it references impl_, which is destroyed below.
    CGDisplayRemoveReconfigurationCallback(displayReconfigurationCallback, impl_.get());

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

    if (screen_id == current_screen_id_ && impl_->stream)
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
    if (impl_->stream_stopped.load(std::memory_order_relaxed))
    {
        // The stream died on its own (e.g. the captured display was disconnected). Report a
        // permanent error so the agent recreates the capturer against the current configuration.
        LOG(ERROR) << "Capture stream is stopped";
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

        const size_t width = CGImageGetWidth(cg_image);
        const size_t height = CGImageGetHeight(cg_image);
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

        CGContextDrawImage(context, CGRectMake(0, 0, width, height), cg_image);
        CGContextRelease(context);

        // The image representation may be larger than the logical size (Retina); the hotspot is
        // reported in points and has to be scaled to the pixel dimensions.
        const NSSize size_points = ns_image.size;
        const qreal scale_x =
            size_points.width > 0 ? static_cast<qreal>(width) / size_points.width : 1.0;
        const qreal scale_y =
            size_points.height > 0 ? static_cast<qreal>(height) / size_points.height : 1.0;

        const NSPoint ns_hotspot = cursor.hotSpot;
        const QPoint hotspot(static_cast<int>(ns_hotspot.x * scale_x),
                             static_cast<int>(ns_hotspot.y * scale_y));

        const QPoint dpi(static_cast<int>(MouseCursor::kDefaultDpiX * scale_x),
                         static_cast<int>(MouseCursor::kDefaultDpiY * scale_y));

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
    SCShareableContent* content = copyShareableContent();
    if (!content)
        return false;

    impl_->displays.clear();
    QRect desktop;

    const CGDirectDisplayID main_display = CGMainDisplayID();

    for (SCDisplay* display in content.displays)
    {
        ScreenCapturerMacImpl::Display info;
        info.id = display.displayID;
        info.frame = cgRectToQRect(display.frame);
        info.primary = (display.displayID == main_display);
        info.title = QStringLiteral("Display %1").arg(display.displayID);

        impl_->displays.push_back(info);
        desktop = desktop.united(info.frame);
    }

    [content release];

    desktop_rect_ = desktop;
    return !impl_->displays.empty();
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

    SCShareableContent* content = copyShareableContent();
    if (!content)
        return false;

    SCDisplay* target = nil;
    for (SCDisplay* display in content.displays)
    {
        if (static_cast<ScreenId>(display.displayID) == current_screen_id_)
        {
            target = display;
            break;
        }
    }

    if (!target)
    {
        LOG(ERROR) << "Display not found for capture:" << current_screen_id_;
        [content release];
        return false;
    }

    current_screen_rect_ = cgRectToQRect(target.frame);

    SCContentFilter* filter =
        [[SCContentFilter alloc] initWithDisplay:target excludingWindows:@[]];

    SCStreamConfiguration* config = [[SCStreamConfiguration alloc] init];
    config.width = static_cast<size_t>(current_screen_rect_.width());
    config.height = static_cast<size_t>(current_screen_rect_.height());
    config.pixelFormat = kCVPixelFormatType_32BGRA;
    config.showsCursor = NO;
    config.queueDepth = 5;

    // Without this ScreenCaptureKit delivers at the display's native refresh (60/120 Hz), and the
    // delegate copies each delivered frame even though the agent consumes at most 30 fps - wasting
    // memory bandwidth that starves the encoder. Cap delivery at the remote-desktop maximum.
    config.minimumFrameInterval = CMTimeMake(1, 30);

    SCStream* stream = [[SCStream alloc] initWithFilter:filter
                                          configuration:config
                                               delegate:impl_->output];
    [filter release];
    [config release];

    NSError* output_error = nil;
    if (![stream addStreamOutput:impl_->output
                            type:SCStreamOutputTypeScreen
              sampleHandlerQueue:impl_->queue
                           error:&output_error])
    {
        LOG(ERROR) << "addStreamOutput failed: "
                   << (output_error ? output_error.localizedDescription.UTF8String : "unknown");
        [stream release];
        [content release];
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
    [content release];

    if (!started)
    {
        [stream release];
        return false;
    }

    // A stop notification from a previous stream instance no longer applies.
    impl_->stream_stopped = false;

    impl_->stream = stream;
    return true;
}

//--------------------------------------------------------------------------------------------------
void ScreenCapturerMac::stopStream()
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
