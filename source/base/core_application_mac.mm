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

#include "base/core_application.h"

#include <QDir>
#include <QProcess>

#include <AppKit/AppKit.h>

#include "base/logging.h"

namespace {

//--------------------------------------------------------------------------------------------------
void startNewInstance()
{
    QString bundle = QDir(QCoreApplication::applicationDirPath() + "/../..").canonicalPath();

    LOG(INFO) << "Start new instance:" << bundle;

    // "-n" is required, because the process asked to open is itself the instance a plain "open"
    // reuses, and nothing would appear on the screen.
    if (!QProcess::startDetached("open", QStringList() << "-n" << bundle))
        LOG(ERROR) << "Unable to start new instance";
}

} // namespace

//--------------------------------------------------------------------------------------------------
@interface AspiaApplicationDelegate : NSObject <NSApplicationDelegate>
@end

@implementation AspiaApplicationDelegate

- (BOOL)applicationShouldHandleReopen:(NSApplication*)application hasVisibleWindows:(BOOL)has_windows
{
    Q_UNUSED(application)
    Q_UNUSED(has_windows)

    LOG(INFO) << "Reopen requested";
    startNewInstance();
    return YES;
}

@end

//--------------------------------------------------------------------------------------------------
int CoreApplication::execAppKitLoop()
{
    LOG(INFO) << "Run the loop of AppKit";

    appkit_loop_ = true;

    [NSApplication sharedApplication];

    // The application does not retain its delegate, and this one is needed as long as the process
    // runs.
    [NSApp setDelegate:[[AspiaApplicationDelegate alloc] init]];

    [NSApp run];

    LOG(INFO) << "The loop of AppKit finished";
    return 0;
}

//--------------------------------------------------------------------------------------------------
// static
void CoreApplication::stopAppKitLoop()
{
    LOG(INFO) << "Stop the loop of AppKit";

    // The stop takes effect when the next event arrives, and an idle process has none, so one is
    // posted.
    [NSApp stop:nil];
    [NSApp postEvent:[NSEvent otherEventWithType:NSEventTypeApplicationDefined
                                        location:NSZeroPoint
                                   modifierFlags:0
                                       timestamp:0
                                    windowNumber:0
                                         context:nil
                                         subtype:0
                                           data1:0
                                           data2:0]
             atStart:YES];
}
