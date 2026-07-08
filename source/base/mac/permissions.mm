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

#include "base/mac/permissions.h"

#include <ApplicationServices/ApplicationServices.h>
#import <AppKit/AppKit.h>

#include <unistd.h>

//--------------------------------------------------------------------------------------------------
static bool hasFullDiskAccess()
{
    // There is no API for Full Disk Access. As a proxy, FDA is what lets a process read the user's TCC
    // database; probe it for read access.
    NSString* path = [NSHomeDirectory()
        stringByAppendingPathComponent:@"Library/Application Support/com.apple.TCC/TCC.db"];
    return access(path.fileSystemRepresentation, R_OK) == 0;
}

//--------------------------------------------------------------------------------------------------
static NSString* settingsPaneUrl(MacPermission permission)
{
    switch (permission)
    {
        case MacPermission::SCREEN_RECORDING:
            return @"x-apple.systempreferences:com.apple.preference.security?Privacy_ScreenCapture";

        case MacPermission::ACCESSIBILITY:
            return @"x-apple.systempreferences:com.apple.preference.security?Privacy_Accessibility";

        case MacPermission::FULL_DISK_ACCESS:
            return @"x-apple.systempreferences:com.apple.preference.security?Privacy_AllFiles";
    }

    return nil;
}

//--------------------------------------------------------------------------------------------------
bool hasMacPermission(MacPermission permission)
{
    switch (permission)
    {
        case MacPermission::SCREEN_RECORDING:
            return CGPreflightScreenCaptureAccess();

        case MacPermission::ACCESSIBILITY:
            return AXIsProcessTrusted();

        case MacPermission::FULL_DISK_ACCESS:
            return hasFullDiskAccess();
    }

    return false;
}

//--------------------------------------------------------------------------------------------------
void requestMacPermission(MacPermission permission)
{
    switch (permission)
    {
        case MacPermission::SCREEN_RECORDING:
            // Prompts once and adds the application to the Screen Recording list; the grant takes
            // effect only after the application is restarted.
            CGRequestScreenCaptureAccess();
            break;

        case MacPermission::ACCESSIBILITY:
        {
            NSDictionary* options = @{ (__bridge id)kAXTrustedCheckOptionPrompt: @YES };
            AXIsProcessTrustedWithOptions((__bridge CFDictionaryRef)options);
            break;
        }

        case MacPermission::FULL_DISK_ACCESS:
            // No request API exists; open the pane so the user can add the application.
            openMacPermissionSettings(permission);
            break;
    }
}

//--------------------------------------------------------------------------------------------------
void openMacPermissionSettings(MacPermission permission)
{
    NSString* url = settingsPaneUrl(permission);
    if (url)
        [[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:url]];
}
