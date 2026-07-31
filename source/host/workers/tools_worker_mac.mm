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
#include "host/workers/tools_worker.h"

#include <QFileInfo>
#include <QProcess>
#include <QSettings>

#include <CoreFoundation/CoreFoundation.h>
#include <ImageIO/ImageIO.h>
#include <pwd.h>

#include "base/logging.h"
#include "base/session_id.h"
#include "base/mac/scoped_cftyperef.h"

namespace {

//--------------------------------------------------------------------------------------------------
// Returns the name of the application as the system shows it. The bundle carries its name in the
// language of the host, so the client gets the same name the local user sees in Finder.
QString bundleName(const QString& bundle_path)
{
    ScopedCFTypeRef<CFStringRef> path(CFStringCreateWithCString(
        nullptr, bundle_path.toUtf8().constData(), kCFStringEncodingUTF8));
    if (!path)
        return QString();

    ScopedCFTypeRef<CFURLRef> url(CFURLCreateWithFileSystemPath(
        nullptr, path.get(), kCFURLPOSIXPathStyle, true));
    if (!url)
        return QString();

    ScopedCFTypeRef<CFBundleRef> bundle(CFBundleCreate(nullptr, url.get()));
    if (!bundle)
        return QString();

    // Returns the value from the localized information dictionary of the bundle.
    CFStringRef name = static_cast<CFStringRef>(
        CFBundleGetValueForInfoDictionaryKey(bundle.get(), kCFBundleNameKey));
    if (!name)
        return QString();

    char buffer[256] = { 0 };
    if (!CFStringGetCString(name, buffer, sizeof(buffer), kCFStringEncodingUTF8))
        return QString();

    return QString::fromUtf8(buffer);
}

//--------------------------------------------------------------------------------------------------
// Returns the icon of the application bundle in PNG format. The icon file holds the image in many
// sizes and formats the image plugins of Qt do not read, so it goes through the system decoder.
QByteArray bundleIcon(const QString& bundle_path)
{
    QSettings info(bundle_path + "/Contents/Info.plist", QSettings::NativeFormat);

    QString icon_name = info.value("CFBundleIconFile").toString();
    if (icon_name.isEmpty())
    {
        LOG(WARNING) << "No icon in" << bundle_path;
        return QByteArray();
    }

    if (!icon_name.endsWith(".icns", Qt::CaseInsensitive))
        icon_name += ".icns";

    const QString icon_path = bundle_path + "/Contents/Resources/" + icon_name;

    ScopedCFTypeRef<CFStringRef> path(CFStringCreateWithCString(
        nullptr, icon_path.toUtf8().constData(), kCFStringEncodingUTF8));
    if (!path)
        return QByteArray();

    ScopedCFTypeRef<CFURLRef> url(CFURLCreateWithFileSystemPath(
        nullptr, path.get(), kCFURLPOSIXPathStyle, false));
    if (!url)
        return QByteArray();

    ScopedCFTypeRef<CGImageSourceRef> source(CGImageSourceCreateWithURL(url.get(), nullptr));
    if (!source)
    {
        LOG(WARNING) << "Unable to read icon" << icon_path;
        return QByteArray();
    }

    // A menu item needs a small image, not the largest one the file has. The same size the system
    // icons have on Windows, so the list costs the same on both.
    const int max_size = 32;
    ScopedCFTypeRef<CFNumberRef> size(CFNumberCreate(nullptr, kCFNumberIntType, &max_size));

    const void* keys[] =
    {
        kCGImageSourceCreateThumbnailFromImageAlways, kCGImageSourceThumbnailMaxPixelSize
    };
    const void* values[] = { kCFBooleanTrue, size.get() };

    ScopedCFTypeRef<CFDictionaryRef> options(CFDictionaryCreate(
        nullptr, keys, values, 2, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

    ScopedCFTypeRef<CGImageRef> image(
        CGImageSourceCreateThumbnailAtIndex(source.get(), 0, options.get()));
    if (!image)
    {
        LOG(WARNING) << "Unable to decode icon" << icon_path;
        return QByteArray();
    }

    ScopedCFTypeRef<CFMutableDataRef> data(CFDataCreateMutable(nullptr, 0));
    ScopedCFTypeRef<CGImageDestinationRef> destination(
        CGImageDestinationCreateWithData(data.get(), CFSTR("public.png"), 1, nullptr));
    if (!destination)
        return QByteArray();

    CGImageDestinationAddImage(destination.get(), image.get(), nullptr);

    if (!CGImageDestinationFinalize(destination.get()))
    {
        LOG(WARNING) << "Unable to encode icon" << icon_path;
        return QByteArray();
    }

    return QByteArray(reinterpret_cast<const char*>(CFDataGetBytePtr(data.get())),
                      static_cast<int>(CFDataGetLength(data.get())));
}

//--------------------------------------------------------------------------------------------------
// Returns the name of the user owning the session, empty if there is no such user.
QString userName(SessionId session_id)
{
    // A session is identified by the uid of the user owning the console.
    struct passwd* pw = getpwuid(static_cast<uid_t>(session_id));
    if (!pw)
        return QString();

    return QString::fromLocal8Bit(pw->pw_name);
}

} // namespace

//--------------------------------------------------------------------------------------------------
void ToolsWorker::buildToolList()
{
    // Adds the application if its bundle is present on this machine. The name and the icon are taken
    // from the bundle, so the client shows exactly what the local user sees in Finder.
    auto add = [this](const QString& name, const QString& bundle_path)
    {
        if (!QFileInfo::exists(bundle_path))
        {
            LOG(INFO) << "Tool" << name << "is not available (" << bundle_path << ")";
            return;
        }

        Tool tool;
        tool.name = bundleName(bundle_path);
        if (tool.name.isEmpty())
            tool.name = name;

        tool.icon = bundleIcon(bundle_path);
        tool.program = bundle_path;

        tools_.append(tool);
    };

    // The settings application was renamed in macOS 13; only one of the two is present.
    add("System Settings", "/System/Applications/System Settings.app");
    add("System Preferences", "/System/Applications/System Preferences.app");

    add("Activity Monitor", "/System/Applications/Utilities/Activity Monitor.app");
    add("Disk Utility", "/System/Applications/Utilities/Disk Utility.app");
    add("System Information", "/System/Applications/Utilities/System Information.app");
    add("Console", "/System/Applications/Utilities/Console.app");
    add("Terminal", "/System/Applications/Utilities/Terminal.app");

    // Keychain Access left the utilities folder in newer systems; only one of the two is present.
    add("Keychain Access", "/System/Applications/Utilities/Keychain Access.app");
    add("Keychain Access", "/System/Library/CoreServices/Applications/Keychain Access.app");
}

//--------------------------------------------------------------------------------------------------
// static
std::span<const ToolsWorker::Script> ToolsWorker::scriptTable()
{
    // Nothing: the scripts are written for the shells of the other systems.
    return {};
}

//--------------------------------------------------------------------------------------------------
bool ToolsWorker::scriptsSupported() const
{
    return false;
}

//--------------------------------------------------------------------------------------------------
bool ToolsWorker::hasInteractiveUser(SessionId session_id) const
{
    // A session is the console user; at the login window there is none. The tool is started in the
    // GUI session of that user, so it must be the one currently owning the console.
    if (session_id == kInvalidSessionId || session_id != activeConsoleSessionId())
        return false;

    return !userName(session_id).isEmpty();
}

//--------------------------------------------------------------------------------------------------
bool ToolsWorker::launchTool(SessionId session_id, const Tool& tool) const
{
    const QString user_name = userName(session_id);
    if (user_name.isEmpty())
    {
        LOG(ERROR) << "No user in session" << session_id;
        return false;
    }

    // The service runs as root outside any GUI session: "launchctl asuser <uid>" puts the tool into
    // the per-user launchd domain (the one with access to WindowServer) and "sudo -u" drops root to
    // that user. It is started through LaunchServices ("open"), just like the user would do it. Every
    // value goes as an argument of its own, so no shell has to be trusted with the path of the tool.
    QStringList arguments;
    arguments << "asuser" << QString::number(session_id) << "sudo" << "-u" << user_name
              << "open" << tool.program;

    LOG(INFO) << "Starting tool" << tool.name << "in session" << session_id
              << "(args:" << arguments << ")";

    QProcess process;
    process.setProgram("launchctl");
    process.setArguments(arguments);
    process.start();

    // "open" hands the bundle over to LaunchServices and returns; waiting for it without an end
    // would hold the worker forever if that answer never comes.
    if (!process.waitForFinished(15000))
    {
        LOG(ERROR) << "Tool launch did not finish in time";
        process.kill();
        process.waitForFinished(1000);
        return false;
    }

    const int exit_code = process.exitCode();
    if (exit_code != 0)
    {
        LOG(ERROR) << "Tool launch failed (code:" << exit_code
                   << "output:" << process.readAllStandardError().trimmed() << ")";
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool ToolsWorker::launchScript(SessionId session_id, const Script& script) const
{
    Q_UNUSED(session_id)
    Q_UNUSED(script)

    LOG(ERROR) << "Running scripts is not implemented for this platform";
    return false;
}
