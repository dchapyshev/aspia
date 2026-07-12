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

#include "host/desktop_resizer_mac.h"

#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>
#import <IOKit/graphics/IOGraphicsTypes.h>

#include <map>

#include "base/logging.h"

namespace {

//--------------------------------------------------------------------------------------------------
struct QSizeLess
{
    bool operator()(const QSize& a, const QSize& b) const
    {
        if (a.width() != b.width())
            return a.width() < b.width();
        return a.height() < b.height();
    }
};

//--------------------------------------------------------------------------------------------------
// Returns true if |candidate| is a better representative of a given logical resolution than
// |current|: prefer the higher pixel density (the Retina variant of a "looks like" size), then the
// higher refresh rate.
bool isBetterMode(CGDisplayModeRef candidate, CGDisplayModeRef current)
{
    const size_t candidate_px = CGDisplayModeGetPixelWidth(candidate);
    const size_t current_px = CGDisplayModeGetPixelWidth(current);

    if (candidate_px != current_px)
        return candidate_px > current_px;

    return CGDisplayModeGetRefreshRate(candidate) > CGDisplayModeGetRefreshRate(current);
}

//--------------------------------------------------------------------------------------------------
// Applies a display mode through a configuration transaction. The change is made kCGConfigureForAppOnly
// so it is temporary: macOS reverts it automatically when this agent process exits (including on a
// crash) and never writes it to the user's permanent display preferences.
bool applyDisplayMode(CGDirectDisplayID display, CGDisplayModeRef mode)
{
    CGDisplayConfigRef config = nullptr;
    if (CGBeginDisplayConfiguration(&config) != kCGErrorSuccess)
    {
        LOG(ERROR) << "CGBeginDisplayConfiguration failed for display" << display;
        return false;
    }

    if (CGConfigureDisplayWithDisplayMode(config, display, mode, nullptr) != kCGErrorSuccess)
    {
        LOG(ERROR) << "CGConfigureDisplayWithDisplayMode failed for display" << display;
        CGCancelDisplayConfiguration(config);
        return false;
    }

    if (CGCompleteDisplayConfiguration(config, kCGConfigureForAppOnly) != kCGErrorSuccess)
    {
        LOG(ERROR) << "CGCompleteDisplayConfiguration failed for display" << display;
        return false;
    }

    return true;
}

} // namespace

//--------------------------------------------------------------------------------------------------
struct DesktopResizerMac::Context
{
    struct Screen
    {
        // Best display mode per logical (point) resolution, retained.
        std::map<QSize, CGDisplayModeRef, QSizeLess> modes;
        // The mode the display had before we first changed it, retained.
        CGDisplayModeRef original = nullptr;
        bool built = false;
        // Whether the resolution was changed and not yet restored.
        bool changed = false;
    };

    std::map<CGDirectDisplayID, Screen> screens;
};

//--------------------------------------------------------------------------------------------------
DesktopResizerMac::DesktopResizerMac()
    : context_(std::make_unique<Context>())
{
    LOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
DesktopResizerMac::~DesktopResizerMac()
{
    LOG(INFO) << "Dtor";

    restoreResulution();

    for (auto& [display_id, screen] : context_->screens)
    {
        for (auto& [size, mode] : screen.modes)
            CGDisplayModeRelease(mode);

        if (screen.original)
            CGDisplayModeRelease(screen.original);
    }
}

//--------------------------------------------------------------------------------------------------
QList<QSize> DesktopResizerMac::supportedResolutions(ScreenId screen_id)
{
    const CGDirectDisplayID display_id = static_cast<CGDirectDisplayID>(screen_id);

    Context::Screen& screen = context_->screens[display_id];

    if (!screen.built)
    {
        // Include the scaled ("looks like") HiDPI modes so the list matches what the user sees in
        // Displays settings, and what the capture geometry (logical points) uses.
        NSDictionary* options =
            @{ (NSString*)kCGDisplayShowDuplicateLowResolutionModes : @YES };
        CFArrayRef modes = CGDisplayCopyAllDisplayModes(display_id, (CFDictionaryRef)options);

        if (modes)
        {
            const CFIndex count = CFArrayGetCount(modes);
            for (CFIndex i = 0; i < count; ++i)
            {
                CGDisplayModeRef mode =
                    static_cast<CGDisplayModeRef>(const_cast<void*>(CFArrayGetValueAtIndex(modes, i)));

                // Only modes that are safe to switch to.
                if (!(CGDisplayModeGetIOFlags(mode) & kDisplayModeSafeFlag))
                    continue;

                const size_t width = CGDisplayModeGetWidth(mode);
                const size_t height = CGDisplayModeGetHeight(mode);
                if (!width || !height)
                    continue;

                const QSize size(static_cast<int>(width), static_cast<int>(height));

                auto existing = screen.modes.find(size);
                if (existing == screen.modes.end())
                {
                    screen.modes[size] = CGDisplayModeRetain(mode);
                }
                else if (isBetterMode(mode, existing->second))
                {
                    CGDisplayModeRelease(existing->second);
                    existing->second = CGDisplayModeRetain(mode);
                }
            }

            CFRelease(modes);
        }
        else
        {
            LOG(ERROR) << "CGDisplayCopyAllDisplayModes failed for display" << display_id;
        }

        screen.original = CGDisplayCopyDisplayMode(display_id);
        screen.built = true;
    }

    QList<QSize> result;
    for (const auto& [size, mode] : screen.modes)
        result.append(size);

    return result;
}

//--------------------------------------------------------------------------------------------------
bool DesktopResizerMac::setResolution(ScreenId screen_id, const QSize& resolution)
{
    // Make sure the mode table for this display is populated.
    supportedResolutions(screen_id);

    const CGDirectDisplayID display_id = static_cast<CGDirectDisplayID>(screen_id);
    Context::Screen& screen = context_->screens[display_id];

    auto mode = screen.modes.find(resolution);
    if (mode == screen.modes.end())
    {
        LOG(ERROR) << "Resolution" << resolution << "not supported by display" << display_id;
        return false;
    }

    if (!applyDisplayMode(display_id, mode->second))
        return false;

    screen.changed = true;
    return true;
}

//--------------------------------------------------------------------------------------------------
void DesktopResizerMac::restoreResolution(ScreenId screen_id)
{
    const CGDirectDisplayID display_id = static_cast<CGDirectDisplayID>(screen_id);

    auto screen = context_->screens.find(display_id);
    if (screen == context_->screens.end() || !screen->second.original || !screen->second.changed)
        return;

    if (applyDisplayMode(display_id, screen->second.original))
        screen->second.changed = false;
}

//--------------------------------------------------------------------------------------------------
void DesktopResizerMac::restoreResulution()
{
    for (const auto& [display_id, screen] : context_->screens)
        restoreResolution(static_cast<ScreenId>(display_id));
}
