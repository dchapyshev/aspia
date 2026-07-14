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

#include "host/desktop_environment_linux.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QTimer>

#include <grp.h>
#include <pwd.h>
#include <unistd.h>

#include <string>
#include <vector>

#include "base/logging.h"
#include "base/linux/session_util.h"

namespace {

const char kBackgroundSchema[] = "org.gnome.desktop.background";
const char kInterfaceSchema[] = "org.gnome.desktop.interface";
const char kPictureOptions[] = "picture-options";
const char kEnableAnimations[] = "enable-animations";

//--------------------------------------------------------------------------------------------------
// Computes the supplementary group list for |user| (with primary group |gid|) in the parent, because
// initgroups()/getgrouplist() are not async-signal-safe. The child applies it with setgroups().
std::vector<gid_t> groupsForUser(const char* user, gid_t gid)
{
    int count = 32;
    std::vector<gid_t> groups(static_cast<size_t>(count));

    if (getgrouplist(user, gid, groups.data(), &count) < 0)
    {
        // The buffer was too small; |count| now holds the required size.
        groups.resize(static_cast<size_t>(count > 0 ? count : 1));
        if (getgrouplist(user, gid, groups.data(), &count) < 0)
            return std::vector<gid_t>();
    }

    groups.resize(static_cast<size_t>(count > 0 ? count : 0));
    return groups;
}

} // namespace

//--------------------------------------------------------------------------------------------------
DesktopEnvironmentLinux::DesktopEnvironmentLinux(QObject* parent)
    : DesktopEnvironment(parent)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
DesktopEnvironmentLinux::~DesktopEnvironmentLinux()
{
    // Restore anything that was changed when the session ends. Applied synchronously because no more
    // event-loop turns will run.
    desired_wallpaper_disabled_ = false;
    desired_effects_disabled_ = false;
    applyDesired();
}

//--------------------------------------------------------------------------------------------------
// static
DesktopEnvironmentLinux::Desktop DesktopEnvironmentLinux::detectDesktop(uid_t uid)
{
    // The logind Desktop property is often empty (not set by every display manager), so detect the
    // desktop from the shell process the user is actually running.
    const QStringList pids = QDir("/proc").entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString& pid : pids)
    {
        bool is_pid = false;
        pid.toUInt(&is_pid);
        if (!is_pid)
            continue;

        const QString dir = "/proc/" + pid;
        if (QFileInfo(dir).ownerId() != uid)
            continue;

        QFile comm(dir + "/comm");
        if (!comm.open(QIODevice::ReadOnly))
            continue;

        const QString name = QString::fromLatin1(comm.readLine()).trimmed();
        if (name == "gnome-shell")
            return Desktop::GNOME;
        if (name == "plasmashell")
            return Desktop::KDE;
    }

    return Desktop::OTHER;
}

//--------------------------------------------------------------------------------------------------
bool DesktopEnvironmentLinux::resolveTarget()
{
    QString session_id;
    uid_t uid = 0;
    if (!SessionUtil::activeSession(&session_id, &uid))
        return false;

    const struct passwd* pw = getpwuid(uid);
    if (!pw)
    {
        PLOG(ERROR) << "getpwuid failed";
        return false;
    }

    uid_ = uid;
    gid_ = pw->pw_gid;
    home_dir_ = QString::fromLocal8Bit(pw->pw_dir);
    user_name_ = QString::fromLocal8Bit(pw->pw_name);
    desktop_ = detectDesktop(uid);

    return !user_name_.isEmpty();
}

//--------------------------------------------------------------------------------------------------
bool DesktopEnvironmentLinux::runAsUser(
    const QString& program, const QStringList& arguments, QString* output)
{
    if (uid_ == 0)
        return false;

    // Run the command with QProcess (it handles PATH lookup, argv, pipes and waiting). It also builds
    // the child's environment for us, so gsettings/dbus-send reach the target user's session bus.
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("HOME", home_dir_);
    env.insert("USER", user_name_);
    env.insert("LOGNAME", user_name_);
    env.insert("XDG_RUNTIME_DIR", QString("/run/user/%1").arg(uid_));
    env.insert("DBUS_SESSION_BUS_ADDRESS", QString("unix:path=/run/user/%1/bus").arg(uid_));

    // Privileges are dropped in the child via setChildProcessModifier(), which runs after fork() and
    // before exec. There only async-signal-safe calls are allowed, so the supplementary group list is
    // computed here (getgrouplist() is not async-signal-safe) and applied there with setgroups().
    const std::vector<gid_t> groups = groupsForUser(user_name_.toStdString().c_str(), gid_);
    if (groups.empty())
    {
        LOG(ERROR) << "Unable to build group list for user:" << user_name_;
        return false;
    }

    QProcess process;
    process.setProcessEnvironment(env);
    process.setChildProcessModifier([groups, gid = gid_, uid = uid_]()
    {
        // Drop to the session user: supplementary groups first, then gid, then uid, all while still
        // privileged. All of these are async-signal-safe.
        if (setgroups(groups.size(), groups.data()) != 0 || setgid(gid) != 0 || setuid(uid) != 0)
            _exit(127);
    });

    process.start(program, arguments);
    if (!process.waitForFinished(5000))
    {
        LOG(ERROR) << "Timed out running:" << program;
        process.kill();
        process.waitForFinished(1000);
        return false;
    }

    if (output)
        *output = QString::fromUtf8(process.readAllStandardOutput()).trimmed();

    return process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
}

//--------------------------------------------------------------------------------------------------
void DesktopEnvironmentLinux::disableWallpaper()
{
    desired_wallpaper_disabled_ = true;
    scheduleApply();
}

//--------------------------------------------------------------------------------------------------
void DesktopEnvironmentLinux::disableEffects()
{
    desired_effects_disabled_ = true;
    scheduleApply();
}

//--------------------------------------------------------------------------------------------------
void DesktopEnvironmentLinux::revertAll()
{
    desired_wallpaper_disabled_ = false;
    desired_effects_disabled_ = false;
    scheduleApply();
}

//--------------------------------------------------------------------------------------------------
void DesktopEnvironmentLinux::scheduleApply()
{
    if (apply_scheduled_)
        return;

    apply_scheduled_ = true;
    QTimer::singleShot(0, this, [this]()
    {
        apply_scheduled_ = false;
        applyDesired();
    });
}

//--------------------------------------------------------------------------------------------------
void DesktopEnvironmentLinux::applyDesired()
{
    // Nothing changed since the last application: skip resolving the session entirely.
    if (desired_wallpaper_disabled_ == applied_wallpaper_disabled_ &&
        desired_effects_disabled_ == applied_effects_disabled_)
    {
        return;
    }

    if (!resolveTarget())
        return;

    // Wallpaper: GNOME only (KDE wallpaper is persistent and cannot be reverted safely). Toggling
    // picture-options to "none" draws the solid color and never touches the user's picture-uri.
    if (desktop_ == Desktop::GNOME)
    {
        if (desired_wallpaper_disabled_ && !applied_wallpaper_disabled_)
        {
            // Only change anything if the original can be read back for a guaranteed revert.
            QString options;
            if (runAsUser("gsettings",
                          QStringList() << "get" << kBackgroundSchema << kPictureOptions, &options))
            {
                // Never capture an already-disabled value as the "original". A session that could not
                // revert on logout (its user bus was already gone) leaves picture-options at "none" in the
                // persistent dconf; saving that and later restoring it to "none" would lose the wallpaper
                // for good. When it is already "none", reset to the schema default on revert instead.
                saved_picture_options_ =
                    (options == "'none'" || options == "none") ? QString() : options;
                runAsUser("gsettings",
                          QStringList() << "set" << kBackgroundSchema << kPictureOptions << "none",
                          nullptr);
                applied_wallpaper_disabled_ = true;
                LOG(INFO) << "GNOME wallpaper disabled";
            }
        }
        else if (!desired_wallpaper_disabled_ && applied_wallpaper_disabled_)
        {
            if (saved_picture_options_.isEmpty())
            {
                // No valid original was captured (it was already "none"): reset to the schema default so
                // the wallpaper reappears - picture-uri was never touched.
                runAsUser("gsettings",
                          QStringList() << "reset" << kBackgroundSchema << kPictureOptions, nullptr);
            }
            else
            {
                runAsUser("gsettings", QStringList()
                    << "set" << kBackgroundSchema << kPictureOptions << saved_picture_options_, nullptr);
            }
            applied_wallpaper_disabled_ = false;
            LOG(INFO) << "GNOME wallpaper restored";
        }
    }

    if (desired_effects_disabled_ && !applied_effects_disabled_)
    {
        if (desktop_ == Desktop::GNOME)
        {
            QString animations;
            if (runAsUser("gsettings",
                          QStringList() << "get" << kInterfaceSchema << kEnableAnimations, &animations))
            {
                saved_animations_ = animations;
                runAsUser("gsettings",
                          QStringList() << "set" << kInterfaceSchema << kEnableAnimations << "false",
                          nullptr);
                applied_effects_disabled_ = true;
                LOG(INFO) << "GNOME animations disabled";
            }
        }
        else if (desktop_ == Desktop::KDE)
        {
            // Runtime-only compositor toggle: touches no configuration file, no-op on Wayland.
            runAsUser("dbus-send", QStringList()
                << "--session" << "--type=method_call" << "--dest=org.kde.KWin"
                << "/Compositor" << "org.kde.kwin.Compositing.suspend", nullptr);
            applied_effects_disabled_ = true;
            LOG(INFO) << "KDE compositor suspended";
        }
    }
    else if (!desired_effects_disabled_ && applied_effects_disabled_)
    {
        if (desktop_ == Desktop::GNOME)
        {
            runAsUser("gsettings", QStringList()
                << "set" << kInterfaceSchema << kEnableAnimations << saved_animations_, nullptr);
        }
        else if (desktop_ == Desktop::KDE)
        {
            runAsUser("dbus-send", QStringList()
                << "--session" << "--type=method_call" << "--dest=org.kde.KWin"
                << "/Compositor" << "org.kde.kwin.Compositing.resume", nullptr);
        }

        applied_effects_disabled_ = false;
        LOG(INFO) << "Effects restored";
    }
}
