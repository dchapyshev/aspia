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
#include <QFileInfo>
#include <QProcess>

#include <AppKit/AppKit.h>

#include "base/logging.h"
#include "base/process_util.h"

namespace {

int g_appkit_exit_code = 0;

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

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication*)application
{
    Q_UNUSED(application)

    NSAppleEventDescriptor* event = [[NSAppleEventManager sharedAppleEventManager] currentAppleEvent];
    const OSType reason = [[event attributeDescriptorForKeyword:kAEQuitReason] enumCodeValue];
    const pid_t sender = [[event attributeDescriptorForKeyword:keySenderPIDAttr] int32Value];
    const QString sender_path = sender > 0 ? ProcessUtil::filePath(static_cast<quint32>(sender)) : QString();

    LOG(INFO) << "Quit requested (reason:" << QString::number(reason, 16) << "sender:" << sender_path << ")";

    // The end of the session comes from loginwindow and is left as it is. The system restarting the
    // application to apply a privacy permission just given sends the same reasons.
    if (QFileInfo(sender_path).fileName() == "loginwindow")
        return NSTerminateNow;

    // launchd starts this process again only after a failed exit.
    g_appkit_exit_code = 1;
    CoreApplication::quit();
    return NSTerminateCancel;
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
    return g_appkit_exit_code;
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
