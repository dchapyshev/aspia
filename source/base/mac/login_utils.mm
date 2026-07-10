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

#include "base/mac/login_utils.h"

#import <Foundation/Foundation.h>
#import <SystemConfiguration/SystemConfiguration.h>

#include "base/logging.h"
#include "base/ipc/ipc_server.h"

namespace {

//--------------------------------------------------------------------------------------------------
const char kAgentLabel[] = "org.aspia.host-loginwindow";
const char kAgentPlistPath[] = "/Library/LaunchAgents/org.aspia.host-loginwindow.plist";

//--------------------------------------------------------------------------------------------------
NSString* toNSString(const QString& string)
{
    return [NSString stringWithUTF8String:string.toUtf8().constData()];
}

} // namespace

//--------------------------------------------------------------------------------------------------
const SessionId LoginUtils::kSessionId = -2;

//--------------------------------------------------------------------------------------------------
// static
bool LoginUtils::isActive()
{
    SCDynamicStoreRef store = SCDynamicStoreCreate(nullptr, CFSTR("aspia"), nullptr, nullptr);
    if (!store)
        return false;

    CFStringRef user = SCDynamicStoreCopyConsoleUser(store, nullptr, nullptr);
    CFRelease(store);

    if (!user)
        return false;

    const bool result = CFEqual(user, CFSTR("loginwindow"));
    CFRelease(user);
    return result;
}

//--------------------------------------------------------------------------------------------------
// static
bool LoginUtils::installAgent(const QString& app_path)
{
    // The desktop-capture agent is the same aspia_host binary. launchd loads it into whichever GUI
    // session is active - the login window (as root) or a logged-in user's Aqua session (as that user)
    // - always with a proper WindowServer association, and reloads it on every session switch. This is
    // what lets capture follow the login screen and user sessions without the service spawning anything
    // itself. It rendezvous with the service on the fixed channel, retrying until the service serves it.
    NSDictionary* plist = @{
        @"Label" : @(kAgentLabel),
        @"LimitLoadToSessionType" : @[ @"Aqua", @"LoginWindow" ],
        @"ProgramArguments" : @[ toNSString(app_path), @"--session-type", @"desktop" ],
        @"EnvironmentVariables" : @{
            @(IpcServer::kChannelIdEnvVar) : @(LoginUtils::kChannelId),
            @"ASPIA_IPC_RETRY" : @"1",
            @"ASPIA_LOG_TO_FILE" : @"1"
        },
        @"RunAtLoad" : @YES,
        // Relaunch only on a crash/error exit (as RustDesk does), not on a clean exit, so a session
        // switch or intentional stop does not spin the process under launchd.
        @"KeepAlive" : @{ @"SuccessfulExit" : @NO },
        @"ThrottleInterval" : @1,
        @"ProcessType" : @"Interactive"
        // No StandardError/OutPath: the agent runs as root (login window) or the user (Aqua) at
        // different times; a single shared log file ends up owned by one and makes launchd fail to
        // spawn the other (EX_CONFIG). The agent logs to $TMPDIR/aspia when ASPIA_LOG_TO_FILE is set.
    };

    NSString* path = @(kAgentPlistPath);
    if (![plist writeToFile:path atomically:YES])
    {
        LOG(ERROR) << "Failed to write the login-window agent plist";
        return false;
    }

    // launchd requires the plist to be owned by root and not writable by others.
    NSDictionary* attributes = @{
        NSFileOwnerAccountID : @0,
        NSFileGroupOwnerAccountID : @0,
        NSFilePosixPermissions : @0644
    };
    [[NSFileManager defaultManager] setAttributes:attributes ofItemAtPath:path error:nil];

    LOG(INFO) << "Login-window agent installed";
    return true;
}

//--------------------------------------------------------------------------------------------------
// static
bool LoginUtils::removeAgent()
{
    NSString* path = @(kAgentPlistPath);
    NSFileManager* manager = [NSFileManager defaultManager];

    if (![manager fileExistsAtPath:path])
        return true;

    NSError* error = nil;
    if (![manager removeItemAtPath:path error:&error])
    {
        LOG(ERROR) << "Failed to remove the login-window agent plist: "
                   << error.localizedDescription.UTF8String;
        return false;
    }

    LOG(INFO) << "Login-window agent removed";
    return true;
}
