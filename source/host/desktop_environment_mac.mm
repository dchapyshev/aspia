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

#include "host/desktop_environment_mac.h"

#import <AppKit/AppKit.h>

#include "base/logging.h"

// The project is built without ARC (manual retain/release).

namespace {

//--------------------------------------------------------------------------------------------------
// Runs |block| on the main thread. AppKit desktop/display calls must not run on a worker thread, and
// the DesktopEnvironment is owned by the capture worker.
void runOnMain(void (^block)(void))
{
    if ([NSThread isMainThread])
        block();
    else
        dispatch_sync(dispatch_get_main_queue(), block);
}

//--------------------------------------------------------------------------------------------------
// Fill value for the replacement wallpaper. A neutral mid-gray rather than black, so a dark terminal
// or editor does not blend into the background.
const int kSolidGray = 0x66;

//--------------------------------------------------------------------------------------------------
// Path to a small solid-gray PNG, generated once in the temporary directory. Returned empty on
// failure.
NSString* solidWallpaperPath()
{
    NSString* path = [NSTemporaryDirectory() stringByAppendingPathComponent:@"aspia_wallpaper_solid.png"];
    if ([[NSFileManager defaultManager] fileExistsAtPath:path])
        return path;

    NSBitmapImageRep* rep =
        [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:nullptr
                                                pixelsWide:16
                                                pixelsHigh:16
                                             bitsPerSample:8
                                           samplesPerPixel:3
                                                  hasAlpha:NO
                                                  isPlanar:NO
                                            colorSpaceName:NSCalibratedRGBColorSpace
                                               bytesPerRow:0
                                              bitsPerPixel:0];
    if (!rep)
        return @"";

    // RGB with equal channels yields a gray, so a single fill value works.
    memset([rep bitmapData], kSolidGray, static_cast<size_t>([rep bytesPerRow]) * 16);

    NSData* png = [rep representationUsingType:NSBitmapImageFileTypePNG properties:@{}];
    [rep release];

    if (![png writeToFile:path atomically:YES])
        return @"";

    return path;
}

} // namespace

//--------------------------------------------------------------------------------------------------
DesktopEnvironmentMac::DesktopEnvironmentMac(QObject* parent)
    : DesktopEnvironment(parent)
{
    LOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
DesktopEnvironmentMac::~DesktopEnvironmentMac()
{
    LOG(INFO) << "Dtor";
    revertAll();
}

//--------------------------------------------------------------------------------------------------
void DesktopEnvironmentMac::disableWallpaper()
{
    LOG(INFO) << "Disable desktop wallpaper";

    runOnMain(^{
        NSString* solid = solidWallpaperPath();
        if ([solid length] == 0)
        {
            LOG(ERROR) << "Unable to create the solid wallpaper image";
            return;
        }

        NSURL* solid_url = [NSURL fileURLWithPath:solid];
        NSWorkspace* workspace = [NSWorkspace sharedWorkspace];

        for (NSScreen* screen in [NSScreen screens])
        {
            NSNumber* number = screen.deviceDescription[@"NSScreenNumber"];
            const quint32 display_id = [number unsignedIntValue];

            // Do not overwrite a previously saved original (disableWallpaper may run again after a
            // display change) - keep the first one.
            if (!saved_wallpaper_.contains(display_id))
            {
                NSURL* current = [workspace desktopImageURLForScreen:screen];
                saved_wallpaper_.insert(display_id,
                                        current ? QString::fromUtf8(current.path.UTF8String) : QString());
            }

            NSError* error = nil;
            if (![workspace setDesktopImageURL:solid_url forScreen:screen options:@{} error:&error])
                LOG(ERROR) << "setDesktopImageURL failed:" << error.localizedDescription.UTF8String;
        }
    });
}

//--------------------------------------------------------------------------------------------------
void DesktopEnvironmentMac::disableEffects()
{
    // Not implemented on macOS.
}

//--------------------------------------------------------------------------------------------------
void DesktopEnvironmentMac::revertAll()
{
    LOG(INFO) << "Reverting desktop environment changes";

    if (saved_wallpaper_.isEmpty())
        return;

    runOnMain(^{
        NSWorkspace* workspace = [NSWorkspace sharedWorkspace];

        for (NSScreen* screen in [NSScreen screens])
        {
            NSNumber* number = screen.deviceDescription[@"NSScreenNumber"];
            const quint32 display_id = [number unsignedIntValue];

            auto it = saved_wallpaper_.constFind(display_id);
            if (it == saved_wallpaper_.constEnd() || it.value().isEmpty())
                continue;

            NSURL* url = [NSURL fileURLWithPath:it.value().toNSString()];
            [workspace setDesktopImageURL:url forScreen:screen options:@{} error:nil];
        }
    });

    saved_wallpaper_.clear();
}
