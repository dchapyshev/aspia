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
#include <QTimer>

#include <grp.h>
#include <pwd.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cstdlib>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/linux/session_util.h"

namespace {

const char kBackgroundSchema[] = "org.gnome.desktop.background";
const char kInterfaceSchema[] = "org.gnome.desktop.interface";
const char kPictureOptions[] = "picture-options";
const char kEnableAnimations[] = "enable-animations";

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

    // Prepare everything the child needs before forking (no allocations after fork).
    const std::string user = user_name_.toStdString();
    const std::string home = home_dir_.toStdString();
    const std::string runtime_dir = "/run/user/" + std::to_string(uid_);
    const std::string dbus_address = "unix:path=" + runtime_dir + "/bus";

    std::vector<std::string> argv_storage;
    argv_storage.push_back(program.toStdString());
    for (const QString& argument : arguments)
        argv_storage.push_back(argument.toStdString());

    std::vector<char*> argv;
    argv.reserve(argv_storage.size() + 1);
    for (std::string& argument : argv_storage)
        argv.push_back(argument.data());
    argv.push_back(nullptr);

    int pipe_fd[2] = { -1, -1 };
    if (output && pipe(pipe_fd) != 0)
    {
        PLOG(ERROR) << "pipe failed";
        return false;
    }

    pid_t pid = fork();
    if (pid < 0)
    {
        PLOG(ERROR) << "fork failed";
        if (output)
        {
            ::close(pipe_fd[0]);
            ::close(pipe_fd[1]);
        }
        return false;
    }

    if (pid == 0)
    {
        // Child: redirect stdout to the pipe, drop to the session user, exec on the user bus.
        if (output)
        {
            dup2(pipe_fd[1], STDOUT_FILENO);
            ::close(pipe_fd[0]);
            ::close(pipe_fd[1]);
        }

        if (setsid() == -1)
            _exit(127);
        if (initgroups(user.c_str(), gid_) != 0)
            _exit(127);
        if (setgid(gid_) != 0)
            _exit(127);
        if (setuid(uid_) != 0)
            _exit(127);

        setenv("HOME", home.c_str(), 1);
        setenv("USER", user.c_str(), 1);
        setenv("LOGNAME", user.c_str(), 1);
        setenv("XDG_RUNTIME_DIR", runtime_dir.c_str(), 1);
        setenv("DBUS_SESSION_BUS_ADDRESS", dbus_address.c_str(), 1);

        execvp(argv[0], argv.data());
        _exit(127);
    }

    // Parent.
    if (output)
    {
        ::close(pipe_fd[1]);

        QByteArray data;
        char buffer[4096];
        ssize_t count;
        while ((count = ::read(pipe_fd[0], buffer, sizeof(buffer))) > 0)
            data.append(buffer, count);
        ::close(pipe_fd[0]);

        *output = QString::fromUtf8(data).trimmed();
    }

    int status = 0;
    if (waitpid(pid, &status, 0) < 0)
    {
        PLOG(ERROR) << "waitpid failed";
        return false;
    }

    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
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
                saved_picture_options_ = options;
                runAsUser("gsettings",
                          QStringList() << "set" << kBackgroundSchema << kPictureOptions << "none",
                          nullptr);
                applied_wallpaper_disabled_ = true;
                LOG(INFO) << "GNOME wallpaper disabled";
            }
        }
        else if (!desired_wallpaper_disabled_ && applied_wallpaper_disabled_)
        {
            runAsUser("gsettings", QStringList()
                << "set" << kBackgroundSchema << kPictureOptions << saved_picture_options_, nullptr);
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
