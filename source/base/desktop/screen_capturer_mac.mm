//
// Aspia Project
// Copyright (C) 2016-2024 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/desktop/screen_capturer_mac.h"

#include "base/logging.h"
#include "base/desktop/frame_simple.h"

#include <ApplicationServices/ApplicationServices.h>
#include <CoreFoundation/CoreFoundation.h>
#include <Cocoa/Cocoa.h>

namespace base {

namespace {

const int kBytesPerPixel = 4;

// Copy pixels in the `rect` from `src_place` to `dest_plane`. `rect` should be
// relative to the origin of `src_plane` and `dest_plane`.
void copyRect(const uint8_t* src_plane, int src_plane_stride,
              uint8_t* dest_plane, int dest_plane_stride,
              int bytes_per_pixel, const Rect& rect)
{
    // Get the address of the starting point.
    const int src_y_offset = src_plane_stride * rect.top();
    const int dest_y_offset = dest_plane_stride * rect.top();
    const int x_offset = bytes_per_pixel * rect.left();
    src_plane += src_y_offset + x_offset;
    dest_plane += dest_y_offset + x_offset;

    // Copy pixels in the rectangle line by line.
    const int bytes_per_line = bytes_per_pixel * rect.width();
    const int height = rect.height();

    for (int i = 0; i < height; ++i)
    {
        memcpy(dest_plane, src_plane, bytes_per_line);
        src_plane += src_plane_stride;
        dest_plane += dest_plane_stride;
    }
}

CGImageRef createScaledCGImage(CGImageRef image, int width, int height)
{
    // Create context, keeping original image properties.
    CGColorSpaceRef colorspace = CGImageGetColorSpace(image);
    CGContextRef context = CGBitmapContextCreate(nullptr,
                                                 width,
                                                 height,
                                                 CGImageGetBitsPerComponent(image),
                                                 width * kBytesPerPixel,
                                                 colorspace,
                                                 CGImageGetBitmapInfo(image));

    if (!context)
        return nil;

    // Draw image to context, resizing it.
    CGContextDrawImage(context, CGRectMake(0, 0, width, height), image);
    // Extract resulting image from context.
    CGImageRef imgRef = CGBitmapContextCreateImage(context);
    CGContextRelease(context);

    return imgRef;
}

float getScaleFactorAtPosition(const MacDesktopConfiguration& desktop_config, const Point& position)
{
    // Find the dpi to physical pixel scale for the screen where the mouse cursor is.
    for (auto it = desktop_config.displays.begin(); it != desktop_config.displays.end(); ++it)
    {
        if (it->bounds.contains(position))
            return it->dip_to_pixel_scale;
    }
    return 1;
}

} // namespace

//--------------------------------------------------------------------------------------------------
ScreenCapturerMac::ScreenCapturerMac()
    : ScreenCapturer(ScreenCapturer::Type::MACOSX),
      desktop_frame_provider_(true),
      detect_updated_region_(true),
      desktop_config_monitor_(std::make_shared<DesktopConfigurationMonitor>())
{
    DETACH_FROM_THREAD(thread_checker_);

    desktop_config_ = desktop_config_monitor_->desktopConfiguration();

    // Start and operate CGDisplayStream handler all from capture thread.
    if (!registerRefreshAndMoveHandlers())
    {
        LOG(LS_ERROR) << "Failed to register refresh and move handlers.";
        return;
    }

    screenConfigurationChanged();
}

//--------------------------------------------------------------------------------------------------
ScreenCapturerMac::~ScreenCapturerMac()
{
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    releaseBuffers();
    unregisterRefreshAndMoveHandlers();
}

//--------------------------------------------------------------------------------------------------
int ScreenCapturerMac::screenCount()
{
    return desktop_config_.displays.size();
}

//--------------------------------------------------------------------------------------------------
bool ScreenCapturerMac::screenList(ScreenList* screens)
{
    DCHECK(screens->screens.size() == 0);

    for (size_t i = 0; i < desktop_config_.displays.size(); ++i)
    {
        const MacDisplayConfiguration& config = desktop_config_.displays[i];

        Screen screen;
        screen.id = config.id;
        screen.title = "Display #" + std::to_string(i);
        screen.position = config.pixel_bounds.topLeft();
        screen.resolution = config.pixel_bounds.size();
        screen.dpi = Point(Frame::kStandardDPI * dip_to_pixel_scale_,
                           Frame::kStandardDPI * dip_to_pixel_scale_);
        screen.is_primary = config.is_primary_;

        screens->screens.emplace_back(screen);
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool ScreenCapturerMac::selectScreen(ScreenId screen_id)
{
    if (screen_id == kFullDesktopScreenId)
    {
        current_display_ = 0;
    }
    else
    {
        const MacDisplayConfiguration* config =
            desktop_config_.findDisplayConfigurationById(static_cast<CGDirectDisplayID>(screen_id));
        if (!config)
            return false;
        current_display_ = config->id;
    }

    screenConfigurationChanged();
    current_screen_id_ = screen_id;
    return true;
}

//--------------------------------------------------------------------------------------------------
ScreenCapturer::ScreenId ScreenCapturerMac::currentScreen() const
{
    return current_screen_id_;
}

//--------------------------------------------------------------------------------------------------
const Frame* ScreenCapturerMac::captureFrame(Error* error)
{
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    DCHECK(error);

    queue_.moveToNextFrame();

    if (queue_.currentFrame() && queue_.currentFrame()->isShared())
    {
        LOG(LS_ERROR) << "Overwriting frame that is still shared.";
    }

    MacDesktopConfiguration new_config = desktop_config_monitor_->desktopConfiguration();
    if (!desktop_config_.equals(new_config))
    {
        desktop_config_ = new_config;

        // If the display configuraiton has changed then refresh capturer data structures.
        // Occasionally, the refresh and move handlers are lost when the screen mode changes, so
        // re-register them here.
        unregisterRefreshAndMoveHandlers();
        if (!registerRefreshAndMoveHandlers())
        {
            LOG(LS_ERROR) << "Failed to register refresh and move handlers.";
            *error = Error::PERMANENT;
            return nullptr;
        }
        screenConfigurationChanged();
    }

    // When screen is zoomed in/out, OSX only updates the part of Rects currently displayed on
    // screen, with relative location to current top-left on screen. This will cause problems when
    // we copy the dirty regions to the captured image. So we invalidate the whole screen to copy
    // all the screen contents. With CGI method, the zooming will be ignored and the whole screen
    // contents will be captured as before.
    // With IOSurface method, the zoomed screen contents will be captured.
    if (UAZoomEnabled())
    {
        helper_.invalidateScreen(screen_pixel_bounds_.size());
    }

    Region region;
    helper_.takeInvalidRegion(&region);

    // If the current buffer is from an older generation then allocate a new one. Note that we can't
    // reallocate other buffers at this point, since the caller may still be reading from them.
    if (!queue_.currentFrame())
        queue_.replaceCurrentFrame(SharedFrame::wrap(createFrame()));

    Frame* current_frame = queue_.currentFrame();

    if (!cgBlit(*current_frame, region))
    {
        *error = Error::PERMANENT;
        return nullptr;
    }

    std::unique_ptr<Frame> new_frame = queue_.currentFrame()->share();
    if (detect_updated_region_)
    {
        *new_frame->updatedRegion() = region;
    }
    else
    {
        new_frame->updatedRegion()->addRect(Rect::makeSize(new_frame->size()));
    }

    if (current_display_)
    {
        const MacDisplayConfiguration* config =
            desktop_config_.findDisplayConfigurationById(current_display_);
        if (config)
        {
            new_frame->setTopLeft(
                config->bounds.topLeft().subtract(desktop_config_.bounds.topLeft()));
        }
    }

    helper_.setSizeMostRecent(new_frame->size());

    *error = Error::SUCCEEDED;
    return current_frame;
}

//--------------------------------------------------------------------------------------------------
const MouseCursor* ScreenCapturerMac::captureCursor()
{
    CGEventRef event = CGEventCreate(nullptr);
    CGPoint cursor_position = CGEventGetLocation(event);
    CFRelease(event);

    MacDesktopConfiguration configuration = desktop_config_monitor_->desktopConfiguration();
    float scale = getScaleFactorAtPosition(
        configuration, Point(cursor_position.x, cursor_position.y));

    NSCursor* nscursor = [NSCursor currentSystemCursor];
    NSImage* nsimage = [nscursor image];
    if (nsimage == nil || !nsimage.isValid)
        return nullptr;

    NSSize nssize = [nsimage size]; // DIP size

    // No need to caputre cursor image if it's unchanged since last capture.
    if ([[nsimage TIFFRepresentation] isEqual:[last_ns_cursor_ TIFFRepresentation]])
        return nullptr;

    last_ns_cursor_ = nsimage;

    Size size(round(nssize.width * scale), round(nssize.height * scale));

    NSPoint nshotspot = [nscursor hotSpot];
    Point hotspot(std::max(0, std::min(size.width(), static_cast<int>(nshotspot.x * scale))),
                  std::max(0, std::min(size.height(), static_cast<int>(nshotspot.y * scale))));

    CGImageRef cg_image = [nsimage CGImageForProposedRect:NULL context:nil hints:nil];
    if (!cg_image)
        return nullptr;

    // Before 10.12, OSX may report 1X cursor on Retina screen. (See crbug.com/632995.) After 10.12,
    // OSX may report 2X cursor on non-Retina screen. (See crbug.com/671436.) So scaling the cursor
    // if needed.
    CGImageRef scaled_cg_image = nil;
    if (CGImageGetWidth(cg_image) != static_cast<size_t>(size.width()))
    {
        scaled_cg_image = createScaledCGImage(cg_image, size.width(), size.height());
        if (scaled_cg_image != nil)
            cg_image = scaled_cg_image;
    }

    if (CGImageGetBitsPerPixel(cg_image) != kBytesPerPixel * 8 ||
        CGImageGetWidth(cg_image) != static_cast<size_t>(size.width()) ||
        CGImageGetBitsPerComponent(cg_image) != 8)
    {
        if (scaled_cg_image != nil)
            CGImageRelease(scaled_cg_image);
        return nullptr;
    }

    CGDataProviderRef provider = CGImageGetDataProvider(cg_image);
    CFDataRef image_data_ref = CGDataProviderCopyData(provider);
    if (image_data_ref == nullptr)
    {
        if (scaled_cg_image != nil)
            CGImageRelease(scaled_cg_image);
        return nullptr;
    }

    base::ByteArray image;
    image.resize(size.width() * size.height() * kBytesPerPixel);

    const uint8_t* src_data = reinterpret_cast<const uint8_t*>(CFDataGetBytePtr(image_data_ref));
    int src_stride = CGImageGetBytesPerRow(cg_image);

    uint8_t* dst_data = image.data();
    int dst_stride = size.width() * kBytesPerPixel;

    for (int y = 0; y < size.height(); ++y)
    {
        memcpy(dst_data, src_data, size.width() * kBytesPerPixel);
        src_data += src_stride;
        dst_data += dst_stride;
    }

    CFRelease(image_data_ref);
    if (scaled_cg_image != nil)
        CGImageRelease(scaled_cg_image);

    last_cursor_ = std::make_unique<MouseCursor>(std::move(image), size, hotspot);
    return last_cursor_.get();
}

//--------------------------------------------------------------------------------------------------
Point ScreenCapturerMac::cursorPosition()
{
    Point offset;
    float scale;

    if (current_display_)
    {
        const MacDisplayConfiguration* display_config =
            desktop_config_.findDisplayConfigurationById(current_display_);
        if (!display_config)
        {
            LOG(LS_ERROR) << "Unable to get config for current display: " << current_display_;
            return Point();
        }

        offset = display_config->bounds.topLeft();
        scale = display_config->dip_to_pixel_scale;
    }
    else
    {
        offset = desktop_config_.bounds.topLeft();
        scale = desktop_config_.dip_to_pixel_scale;
    }

    ScopedCFTypeRef<CGEventRef> event(CGEventCreate(nullptr));
    CGPoint cursor_position = CGEventGetLocation(event.get());

    int x = (cursor_position.x - offset.x()) * scale;
    int y = (cursor_position.y - offset.y()) * scale;

    return Point(x, y);
}

//--------------------------------------------------------------------------------------------------
void ScreenCapturerMac::reset()
{
    releaseBuffers();
}

//--------------------------------------------------------------------------------------------------
bool ScreenCapturerMac::cgBlit(const Frame& frame, const Region& region)
{
    // If not all screen region is dirty, copy the entire contents of the previous capture buffer,
    // to capture over.
    if (queue_.previousFrame() && !region.equals(Region(screen_pixel_bounds_)))
    {
        memcpy(frame.frameData(),
               queue_.previousFrame()->frameData(),
               frame.stride() * frame.size().height());
    }

    MacDisplayConfigurations displays_to_capture;
    if (current_display_)
    {
        // Capturing a single screen. Note that the screen id may change when screens are added or
        // removed.
        const MacDisplayConfiguration* config =
            desktop_config_.findDisplayConfigurationById(current_display_);
        if (config)
        {
            displays_to_capture.push_back(*config);
        }
        else
        {
            LOG(LS_ERROR) << "The selected screen cannot be found for capturing.";
            return false;
        }
    }
    else
    {
        // Capturing the whole desktop.
        displays_to_capture = desktop_config_.displays;
    }

    for (size_t i = 0; i < displays_to_capture.size(); ++i)
    {
        const MacDisplayConfiguration& display_config = displays_to_capture[i];

        // Capturing mixed-DPI on one surface is hard, so we only return displays that match the
        // "primary" display's DPI. The primary display is always the first in the list.
        if (i > 0 && display_config.dip_to_pixel_scale != displays_to_capture[0].dip_to_pixel_scale)
        {
            continue;
        }
        // Determine the display's position relative to the desktop, in pixels.
        Rect display_bounds = display_config.pixel_bounds;
        display_bounds.translate(-screen_pixel_bounds_.left(), -screen_pixel_bounds_.top());

        // Determine which parts of the blit region, if any, lay within the monitor.
        Region copy_region = region;
        copy_region.intersectWith(display_bounds);
        if (copy_region.isEmpty())
            continue;

        // Translate the region to be copied into display-relative coordinates.
        copy_region.translate(-display_bounds.left(), -display_bounds.top());

        std::unique_ptr<Frame> frame_source =
            desktop_frame_provider_.takeLatestFrameForDisplay(display_config.id);
        if (!frame_source)
            continue;

        const uint8_t* display_base_address = frame_source->frameData();
        int src_bytes_per_row = frame_source->stride();
        DCHECK(display_base_address);

        // `frame_source` size may be different from display_bounds in case the screen was
        // resized recently.
        copy_region.intersectWith(frame_source->rect());

        // Copy the dirty region from the display buffer into our desktop buffer.
        uint8_t* out_ptr = frame.frameDataAtPos(display_bounds.topLeft());
        for (Region::Iterator it(copy_region); !it.isAtEnd(); it.advance())
        {
            copyRect(display_base_address, src_bytes_per_row,
                     out_ptr, frame.stride(),
                     kBytesPerPixel, it.rect());
        }
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
void ScreenCapturerMac::screenConfigurationChanged()
{
    if (current_display_)
    {
        const MacDisplayConfiguration* config =
            desktop_config_.findDisplayConfigurationById(current_display_);
        screen_pixel_bounds_ = config ? config->pixel_bounds : Rect();
        dip_to_pixel_scale_ = config ? config->dip_to_pixel_scale : 1.0f;
    }
    else
    {
        screen_pixel_bounds_ = desktop_config_.pixel_bounds;
        dip_to_pixel_scale_ = desktop_config_.dip_to_pixel_scale;
    }

    // Release existing buffers, which will be of the wrong size.
    releaseBuffers();

    // Clear the dirty region, in case the display is down-sizing.
    helper_.clearInvalidRegion();

    // Re-mark the entire desktop as dirty.
    helper_.invalidateScreen(screen_pixel_bounds_.size());

    // Make sure the frame buffers will be reallocated.
    queue_.reset();
}

//--------------------------------------------------------------------------------------------------
bool ScreenCapturerMac::registerRefreshAndMoveHandlers()
{
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

    desktop_config_ = desktop_config_monitor_->desktopConfiguration();

    for (const auto& config : desktop_config_.displays)
    {
        size_t pixel_width = config.pixel_bounds.width();
        size_t pixel_height = config.pixel_bounds.height();

        if (pixel_width == 0 || pixel_height == 0)
            continue;

        CGDirectDisplayID display_id = config.id;
        Point display_origin = config.pixel_bounds.topLeft();

        CGDisplayStreamFrameAvailableHandler handler =
            ^(CGDisplayStreamFrameStatus status,
              uint64_t display_time,
              IOSurfaceRef frame_surface,
              CGDisplayStreamUpdateRef updateRef)
        {
            //DCHECK(thread_checker_.IsCurrent());
            if (status == kCGDisplayStreamFrameStatusStopped)
                return;

            // Only pay attention to frame updates.
            if (status != kCGDisplayStreamFrameStatusFrameComplete)
                return;

            size_t count = 0;
            const CGRect* rects = CGDisplayStreamUpdateGetRects(
                updateRef, kCGDisplayStreamUpdateDirtyRects, &count);
            if (count != 0)
            {
                // According to CGDisplayStream.h, it's safe to call
                // CGDisplayStreamStop() from within the callback.
                screenRefresh(display_id, count, rects, display_origin, frame_surface);
            }
        };

        ScopedCFTypeRef<CFDictionaryRef> properties_dict(
            CFDictionaryCreate(kCFAllocatorDefault,
                               (const void*[]){kCGDisplayStreamShowCursor},
                               (const void*[]){kCFBooleanFalse},
                               1,
                               &kCFTypeDictionaryKeyCallBacks,
                               &kCFTypeDictionaryValueCallBacks));

        CGDisplayStreamRef display_stream = CGDisplayStreamCreate(
            display_id, pixel_width, pixel_height, 'BGRA', properties_dict.get(), handler);

        if (display_stream)
        {
            CGError error = CGDisplayStreamStart(display_stream);
            if (error != kCGErrorSuccess)
                return false;

            CFRunLoopSourceRef source = CGDisplayStreamGetRunLoopSource(display_stream);
            CFRunLoopAddSource(CFRunLoopGetCurrent(), source, kCFRunLoopCommonModes);
            display_streams_.push_back(display_stream);
        }
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
void ScreenCapturerMac::unregisterRefreshAndMoveHandlers()
{
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

    for (CGDisplayStreamRef stream : display_streams_)
    {
        CFRunLoopSourceRef source = CGDisplayStreamGetRunLoopSource(stream);
        CFRunLoopRemoveSource(CFRunLoopGetCurrent(), source, kCFRunLoopCommonModes);
        CGDisplayStreamStop(stream);
        CFRelease(stream);
    }
    display_streams_.clear();

    // Release obsolete io surfaces.
    desktop_frame_provider_.release();
}

//--------------------------------------------------------------------------------------------------
void ScreenCapturerMac::screenRefresh(CGDirectDisplayID display_id, CGRectCount count,
    const CGRect* rect_array, const Point& display_origin, IOSurfaceRef io_surface)
{
    if (screen_pixel_bounds_.isEmpty())
        screenConfigurationChanged();

    // The refresh rects are in display coordinates. We want to translate to framebuffer coordinates.
    // If a specific display is being captured, then no change is necessary. If all displays are
    // being captured, then we want to translate by the origin of the display.
    Point translate_vector;
    if (!current_display_)
        translate_vector = display_origin;

    Region region;
    for (CGRectCount i = 0; i < count; ++i)
    {
        // All rects are already in physical pixel coordinates.
        Rect rect = Rect::makeXYWH(rect_array[i].origin.x,
                                   rect_array[i].origin.y,
                                   rect_array[i].size.width,
                                   rect_array[i].size.height);

        rect.translate(translate_vector);
        region.addRect(rect);
    }

    // Always having the latest iosurface before invalidating a region.
    // See https://bugs.chromium.org/p/webrtc/issues/detail?id=8652 for details.
    desktop_frame_provider_.invalidateIOSurface(
        display_id, ScopedCFTypeRef<IOSurfaceRef>(io_surface, RetainPolicy::RETAIN));
    helper_.invalidateRegion(region);
}

//--------------------------------------------------------------------------------------------------
void ScreenCapturerMac::releaseBuffers()
{
    // The buffers might be in use by the encoder, so don't delete them here. Instead, mark them as
    // "needs update"; next time the buffers are used by the capturer, they will be recreated if necessary.
    queue_.reset();
}

//--------------------------------------------------------------------------------------------------
std::unique_ptr<Frame> ScreenCapturerMac::createFrame()
{
    std::unique_ptr<Frame> frame =
        FrameSimple::create(screen_pixel_bounds_.size(), PixelFormat::ARGB());
    frame->setDpi(Point(Frame::kStandardDPI * dip_to_pixel_scale_,
                        Frame::kStandardDPI * dip_to_pixel_scale_));
    return frame;
}

} // namespace base
