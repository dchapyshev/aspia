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

#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QImage>
#include <QLocale>
#include <QPainter>
#include <QProcess>
#include <QStandardPaths>
#include <QSvgRenderer>

#include <pwd.h>
#include <sys/types.h>

#include "base/logging.h"
#include "base/linux/session_util.h"

namespace {

// Where the system keeps the files of the applications installed on it.
const char* kDataDirs[] = { "/usr/local/share", "/usr/share" };

// The terminal emulators a script may be shown in, in the order they are tried, with the arguments
// each of them takes before the command. The ones that run the window and the shell in a single
// process come first: a terminal built as a client of a server (gnome-terminal is one) may hand the
// command to a server belonging to the logged on user, and the script would then run with the rights
// of that user instead of the rights of the service.
const char* kTerminals[][2] =
{
    { "alacritty", "-e" },
    { "kitty", "" },
    { "xterm", "-e" },
    { "konsole", "--separate -e" },
    { "xfce4-terminal", "--disable-server -x" },
    { "mate-terminal", "--disable-factory -x" },
    { "gnome-terminal", "--" }
};

//--------------------------------------------------------------------------------------------------
// Returns the terminal emulator to show a script in, empty if this machine has none, and the
// arguments it takes before the command in |arguments|.
QString scriptTerminal(QStringList* arguments)
{
    for (const auto& terminal : kTerminals)
    {
        const QString program = QStandardPaths::findExecutable(terminal[0]);
        if (program.isEmpty())
            continue;

        *arguments = QString(terminal[1]).split(' ', Qt::SkipEmptyParts);
        return program;
    }

    return QString();
}

//--------------------------------------------------------------------------------------------------
// Returns the home directory of |uid|, empty if there is no such user.
QString homeDirectory(uid_t uid)
{
    const struct passwd* pw = getpwuid(uid);
    if (!pw)
    {
        PLOG(ERROR) << "getpwuid failed for uid" << uid;
        return QString();
    }

    return QString::fromLocal8Bit(pw->pw_dir);
}

//--------------------------------------------------------------------------------------------------
// Returns the desktop entry named |entry_name|, empty if the system does not have it.
QString desktopEntryFile(const QString& entry_name)
{
    for (const char* data_dir : kDataDirs)
    {
        const QString file_path = QString(data_dir) + "/applications/" + entry_name;
        if (QFileInfo::exists(file_path))
            return file_path;
    }

    return QString();
}

//--------------------------------------------------------------------------------------------------
// Reads the "Desktop Entry" group of the entry: an application is described there by its name, its
// icon and the command that starts it. The keys are taken as they are written, with the language
// suffix included ("Name[ru]"), so a translated value can be picked from them.
QHash<QString, QString> readDesktopEntry(const QString& file_path)
{
    QHash<QString, QString> entry;

    QFile file(file_path);
    if (!file.open(QIODevice::ReadOnly))
    {
        LOG(WARNING) << "Unable to open" << file_path;
        return entry;
    }

    bool in_group = false;

    while (!file.atEnd())
    {
        const QString line = QString::fromUtf8(file.readLine()).trimmed();

        // The groups following the one of the application itself describe the actions of its menu.
        if (line.startsWith('['))
        {
            if (in_group)
                break;

            in_group = (line == "[Desktop Entry]");
            continue;
        }

        const int separator = line.indexOf('=');
        if (!in_group || line.startsWith('#') || separator <= 0)
            continue;

        entry.insert(line.left(separator).trimmed(), line.mid(separator + 1).trimmed());
    }

    return entry;
}

//--------------------------------------------------------------------------------------------------
// Returns the value of |key| in the language of this machine, falling back to the untranslated one:
// the client is shown what the local user sees in their own menu.
QString entryValue(const QHash<QString, QString>& entry, const QString& key)
{
    // An entry names the language either with the country ("ru_RU") or without it ("ru").
    const QString language = QLocale::system().name();

    QString value = entry.value(key + '[' + language + ']');
    if (value.isEmpty())
        value = entry.value(key + '[' + language.left(language.indexOf('_')) + ']');
    if (value.isEmpty())
        value = entry.value(key);

    return value;
}

//--------------------------------------------------------------------------------------------------
// Returns the icon themes of |icons_dir| in the order they are searched.
QStringList iconThemes(const QString& icons_dir)
{
    // An application installs its own icon into "hicolor"; the themes of the desktops provide the
    // standard icons an entry may name instead of one of its own.
    static const char* kPreferredThemes[] = { "hicolor", "Adwaita", "Yaru", "breeze", "Papirus" };

    QStringList themes;

    for (const char* theme : kPreferredThemes)
    {
        if (QFileInfo::exists(icons_dir + '/' + theme))
            themes.append(theme);
    }

    // Whatever else the system has is searched after them: a system often ships the icons of its own
    // desktop in a theme of its own.
    const QStringList installed =
        QDir(icons_dir).entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QString& theme : installed)
    {
        if (!themes.contains(theme))
            themes.append(theme);
    }

    return themes;
}

//--------------------------------------------------------------------------------------------------
// Returns the directories the icons of the applications live in, in the order they are searched.
QStringList iconDirectories()
{
    // The size a menu item needs comes first: a larger icon is scaled down, a smaller one is not
    // scaled up. The vector version of an icon has a directory of its own instead of a size.
    static const char* kSizes[] =
    {
        "32x32", "48x48", "64x64", "128x128", "256x256", "scalable", "32", "48", "64", "128", "256"
    };

    // A tool that configures a part of the system is often given the icon of that part instead of an
    // icon of its own, and those are filed elsewhere than the icons of the applications.
    static const char* kCategories[] = { "apps", "categories", "devices" };

    QStringList directories;

    for (const char* data_dir : kDataDirs)
    {
        const QString icons_dir = QString(data_dir) + "/icons";

        const QStringList themes = iconThemes(icons_dir);
        for (const QString& theme : themes)
        {
            const QString theme_dir = icons_dir + '/' + theme;

            for (const char* size : kSizes)
            {
                for (const char* category : kCategories)
                {
                    // A theme places the size either above the category or below it.
                    directories.append(QString("%1/%2/%3").arg(theme_dir, size, category));
                    directories.append(QString("%1/%2/%3").arg(theme_dir, category, size));
                }
            }
        }

        // An application that files its icon in no theme at all.
        directories.append(QString(data_dir) + "/pixmaps");
    }

    return directories;
}

//--------------------------------------------------------------------------------------------------
// Returns the file of the icon named in a desktop entry. An icon is named without the directory and
// the extension: what it is stored as is up to the theme it is taken from.
QString iconFile(const QString& icon_name)
{
    if (icon_name.isEmpty())
        return QString();

    // An entry may name the file itself instead of an icon of a theme.
    if (icon_name.startsWith('/'))
        return QFileInfo(icon_name).isFile() ? icon_name : QString();

    // The name as it is written may already carry the extension.
    static const char* kExtensions[] = { ".png", ".svg", "" };

    const QStringList directories = iconDirectories();
    for (const QString& directory : directories)
    {
        for (const char* extension : kExtensions)
        {
            const QString file_path = directory + '/' + icon_name + extension;
            if (QFileInfo(file_path).isFile())
                return file_path;
        }
    }

    LOG(WARNING) << "No icon named" << icon_name;
    return QString();
}

//--------------------------------------------------------------------------------------------------
// Returns the icon as PNG in the size a menu item needs: a theme stores it in any size it likes, or
// as a vector image. The same size the icons have on the other systems, so the list costs the same
// everywhere.
QByteArray encodeIcon(const QString& file_path)
{
    if (file_path.isEmpty())
        return QByteArray();

    const int icon_size = 32;
    QImage image;

    if (file_path.endsWith(".svg", Qt::CaseInsensitive))
    {
        QSvgRenderer renderer(file_path);
        if (!renderer.isValid())
        {
            LOG(WARNING) << "Unable to read icon" << file_path;
            return QByteArray();
        }

        image = QImage(icon_size, icon_size, QImage::Format_ARGB32);
        image.fill(Qt::transparent);

        QPainter painter(&image);
        renderer.render(&painter);
    }
    else if (!image.load(file_path))
    {
        LOG(WARNING) << "Unable to read icon" << file_path;
        return QByteArray();
    }

    if (image.width() > icon_size || image.height() > icon_size)
        image = image.scaled(icon_size, icon_size, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    QByteArray buffer;
    QBuffer device(&buffer);

    if (!device.open(QIODevice::WriteOnly) || !image.save(&device, "PNG"))
    {
        LOG(WARNING) << "Unable to encode icon" << file_path;
        return QByteArray();
    }

    return buffer;
}

} // namespace

//--------------------------------------------------------------------------------------------------
void ToolsWorker::buildToolList()
{
    // Adds the first of the desktop entries that is installed on this machine: the same tool is named
    // differently on different systems, and some of them have renamed it. The entry gives the name in
    // the language of this machine and the icon, so the client is shown what the local user sees in
    // their own menu.
    auto add = [this](const QStringList& entry_names)
    {
        for (const QString& entry_name : entry_names)
        {
            const QString file_path = desktopEntryFile(entry_name);
            if (file_path.isEmpty())
                continue;

            const QHash<QString, QString> entry = readDesktopEntry(file_path);

            // The entry of an application may stay behind after it has been removed, so what tells
            // whether the tool is there is the program it starts, not the entry.
            const QString try_exec = entry.value("TryExec");
            if (!try_exec.isEmpty() && QStandardPaths::findExecutable(try_exec).isEmpty())
                continue;

            const QStringList command = QProcess::splitCommand(entry.value("Exec"));
            if (command.isEmpty())
                continue;

            const QString program = QStandardPaths::findExecutable(command.first());
            const QString name = entryValue(entry, "Name");

            if (program.isEmpty() || name.isEmpty())
                continue;

            Tool tool;
            tool.name = name;
            tool.icon = encodeIcon(iconFile(entryValue(entry, "Icon")));
            tool.program = program;

            // Everything the entry passes to the program is kept, except the placeholders for the
            // documents to open: a tool is started with nothing to open.
            for (qsizetype i = 1; i < command.size(); ++i)
            {
                if (!command.at(i).startsWith('%'))
                    tool.arguments.append(command.at(i));
            }

            tools_.append(tool);
            return;
        }

        LOG(INFO) << "Tool" << entry_names.first() << "is not available";
    };

    add(QStringList() << "gnome-system-monitor.desktop" << "org.gnome.SystemMonitor.desktop"
                      << "plasma-systemmonitor.desktop" << "org.kde.ksysguard.desktop"
                      << "xfce4-taskmanager.desktop" << "mate-system-monitor.desktop");
    add(QStringList() << "org.gnome.DiskUtility.desktop" << "gnome-disks.desktop"
                      << "org.kde.partitionmanager.desktop" << "gparted.desktop");
    add(QStringList() << "org.gnome.baobab.desktop" << "baobab.desktop"
                      << "org.kde.filelight.desktop");
    add(QStringList() << "org.gnome.Logs.desktop" << "gnome-logs.desktop");
    add(QStringList() << "kinfocenter.desktop" << "org.kde.kinfocenter.desktop"
                      << "hardinfo.desktop");
    add(QStringList() << "nm-connection-editor.desktop");
    add(QStringList() << "firewall-config.desktop" << "gufw.desktop");
    add(QStringList() << "system-config-printer.desktop");
    add(QStringList() << "system-config-selinux.desktop");
    add(QStringList() << "gnome-control-center.desktop" << "org.gnome.Settings.desktop"
                      << "systemsettings.desktop" << "xfce-settings-manager.desktop"
                      << "mate-control-center.desktop");
    add(QStringList() << "org.gnome.Nautilus.desktop" << "nautilus.desktop"
                      << "org.kde.dolphin.desktop" << "thunar.desktop" << "caja.desktop");
    add(QStringList() << "org.gnome.Terminal.desktop" << "gnome-terminal.desktop"
                      << "org.kde.konsole.desktop" << "xfce4-terminal.desktop"
                      << "mate-terminal.desktop");
}

//--------------------------------------------------------------------------------------------------
void ToolsWorker::buildScriptList()
{
    // A script is shown running in a terminal window on the desktop of the user; a machine with no
    // terminal emulator at all is offered nothing rather than items that open nothing.
    QStringList arguments;
    if (scriptTerminal(&arguments).isEmpty())
    {
        LOG(ERROR) << "No terminal emulator is installed; no scripts are offered";
        return;
    }

    auto add = [this](const QString& name, const QString& description, const QString& command,
                      bool confirm, bool as_user = false)
    {
        Script script;
        script.name = name;
        script.description = description;
        script.command = command;
        script.confirm = confirm;
        script.as_user = as_user;

        scripts_.append(script);
    };

    add("Show Log Errors",
        "Lists what the system logged as an error in the last 24 hours",
        "since='24 hours ago'\n"
        "echo \"Errors in the journal since $(date -d \"$since\" '+%Y-%m-%d %H:%M')\"\n"
        "echo\n"
        "\n"
        "# Priority 'err' takes everything from an emergency down to an ordinary error.\n"
        "journalctl -p err --since \"$since\" --no-pager -q > /tmp/aspia-journal.$$\n"
        "echo \"$(wc -l < /tmp/aspia-journal.$$) line(s)\"\n"
        "echo\n"
        "\n"
        "# Grouped by the process that wrote them: a hundred lines of one broken unit are one\n"
        "# problem, and the count says which of them to read first. The number in brackets is the\n"
        "# process id, so it is cut off to keep the records of one program together.\n"
        "echo 'By source:'\n"
        "awk '{ sub(/(\\[[0-9]+\\])?[]:]*$/, \"\", $5); print \"  \" $5 }' /tmp/aspia-journal.$$ |\n"
        "    sort | uniq -c | sort -rn | head -10\n"
        "echo\n"
        "\n"
        "echo 'Last 20 lines:'\n"
        "tail -20 /tmp/aspia-journal.$$\n"
        "rm -f /tmp/aspia-journal.$$\n",
        false);

    add("Show Failed Services",
        "Lists the services that failed to start or died, and why",
        "systemctl --failed --no-pager\n"
        "\n"
        "# The list itself says nothing about the reason; it is in the log of the unit.\n"
        "for unit in $(systemctl --failed --no-legend --plain | awk '{ print $1 }'); do\n"
        "    echo\n"
        "    echo \"=== $unit\"\n"
        "    journalctl -u \"$unit\" -n 10 --no-pager -q\n"
        "done\n",
        false);

    add("Show Unexpected Shutdowns",
        "Lists the boots and shutdowns of this machine: a boot with no shutdown before it was a "
        "crash",
        "echo 'Boots known to the journal:'\n"
        "journalctl --list-boots --no-pager | tail -10\n"
        "if [ \"$(journalctl --list-boots --no-pager | wc -l)\" -le 1 ]; then\n"
        "    echo\n"
        "    echo 'The journal of this machine is not kept between boots, so only the current one'\n"
        "    echo 'is listed. The records below come from the login database instead.'\n"
        "fi\n"
        "echo\n"
        "\n"
        "echo 'Reboots and shutdowns:'\n"
        "last -x reboot shutdown 2>/dev/null | head -20 || echo '  the login database is empty'\n"
        "echo\n"
        "echo 'A reboot with no shutdown recorded before it is an unexpected one.'\n",
        false);

    add("Flush DNS Cache",
        "Clears the name cache of whichever resolver this machine runs",
        "flushed=no\n"
        "\n"
        "if systemctl is-active --quiet systemd-resolved; then\n"
        "    resolvectl flush-caches\n"
        "    echo 'systemd-resolved: cache flushed'\n"
        "    flushed=yes\n"
        "fi\n"
        "\n"
        "# The others keep no interface of their own for this, so they are restarted instead.\n"
        "for service in nscd dnsmasq unbound; do\n"
        "    if systemctl is-active --quiet $service; then\n"
        "        systemctl restart $service\n"
        "        echo \"$service: restarted\"\n"
        "        flushed=yes\n"
        "    fi\n"
        "done\n"
        "\n"
        "if [ $flushed = no ]; then\n"
        "    echo 'No name cache is running on this machine: nothing to flush'\n"
        "fi\n",
        false);

    add("Synchronize System Time",
        "Resynchronizes the clock of this machine with its time source",
        "if systemctl is-active --quiet chronyd; then\n"
        "    chronyc makestep\n"
        "    chronyc tracking\n"
        "elif systemctl is-active --quiet systemd-timesyncd; then\n"
        "    timedatectl set-ntp true\n"
        "    systemctl restart systemd-timesyncd\n"
        "    sleep 2\n"
        "    timedatectl timesync-status\n"
        "else\n"
        "    echo 'Neither chrony nor systemd-timesyncd is running on this machine'\n"
        "fi\n"
        "\n"
        "echo\n"
        "timedatectl\n",
        false);

    add("Restart Print Spooler",
        "Restarts the printing service and removes everything queued for printing",
        "if ! systemctl cat cups.service > /dev/null 2>&1; then\n"
        "    echo 'The printing service is not installed on this machine'\n"
        "else\n"
        "    cancel -a 2> /dev/null\n"
        "    echo 'Print queue cleared'\n"
        "    systemctl restart cups\n"
        "    echo \"Printing service: $(systemctl is-active cups)\"\n"
        "    lpstat -o 2> /dev/null\n"
        "fi\n",
        true);

    add("Free Up Disk Space",
        "Removes old journals, package caches, temporary files and the cache and trash of the "
        "logged on user",
        "before=$(df -k --output=avail / | tail -1)\n"
        "\n"
        "echo '=== Journal'\n"
        "journalctl --disk-usage\n"
        "journalctl --vacuum-time=7d 2>&1 | tail -2\n"
        "\n"
        "echo\n"
        "echo '=== Package cache'\n"
        "if command -v dnf > /dev/null; then\n"
        "    dnf clean all\n"
        "elif command -v apt-get > /dev/null; then\n"
        "    apt-get clean\n"
        "    echo 'apt: cache cleared'\n"
        "else\n"
        "    echo 'no known package manager'\n"
        "fi\n"
        "\n"
        "echo\n"
        "echo '=== Temporary files'\n"
        "systemd-tmpfiles --clean 2>&1 | tail -2\n"
        "echo 'done'\n"
        "\n"
        "# The paths of the user come from the service: this shell belongs to the system account and\n"
        "# knows nothing about the person the machine is being cleaned for.\n"
        "echo\n"
        "echo \"=== Cache and trash of $SESSION_USER\"\n"
        "du -sh \"$SESSION_HOME/.cache\" 2> /dev/null\n"
        "rm -rf \"$SESSION_HOME/.cache/\"* 2> /dev/null\n"
        "rm -rf \"$SESSION_HOME/.local/share/Trash/\"* 2> /dev/null\n"
        "echo 'cleared'\n"
        "\n"
        "after=$(df -k --output=avail / | tail -1)\n"
        "echo\n"
        "echo \"Freed on /: $(( (after - before) / 1024 )) MB\"\n",
        true);
}

//--------------------------------------------------------------------------------------------------
bool ToolsWorker::hasInteractiveUser(SessionId session_id) const
{
    // The session the client sees is the one the seat shows on its console: the desktop agent is a
    // system unit belonging to no session of its own, so there is no session id to take from it.
    Q_UNUSED(session_id)

    QString session;
    uid_t uid = 0;
    if (!SessionUtil::activeSession(&session, &uid))
        return false;

    // A text console has nowhere to show a window: the tools are graphical applications.
    if (SessionUtil::sessionType(session) == SessionUtil::SessionType::TTY)
        return false;

    // At the login screen the session belongs to the display manager and not to a user: a tool
    // started in it would run as the display manager and be of no use to anybody.
    return SessionUtil::sessionClass(session) == SessionUtil::SessionClass::USER;
}

//--------------------------------------------------------------------------------------------------
bool ToolsWorker::launchTool(SessionId session_id, const Tool& tool) const
{
    Q_UNUSED(session_id)

    QString session;
    uid_t uid = 0;
    if (!SessionUtil::activeSession(&session, &uid))
    {
        LOG(ERROR) << "No active session on seat0";
        return false;
    }

    const QString user_name = SessionUtil::userNameByUid(uid);
    if (user_name.isEmpty())
        return false;

    // The display and its authority cookie are not imported into the user manager on every system;
    // pass them to the tool explicitly (see UserSession::startGui()).
    QString display;
    QString xauthority;

    SessionUtil::readX11Env(uid, session, &display, &xauthority);

    // The service lives outside any session, so the tool is started through the systemd manager of
    // the user: that is what makes it a member of their session, and a tool asking for privileges
    // finds the authentication agent of the session. Started outside it, the request would find no
    // agent and fail. "runuser" drops root to the user, whose manager is reached through their
    // runtime directory.
    // Every value goes as an argument of its own instead of a shell command line. The display and
    // its cookie are read out of the processes of the logged on user, so they are whatever that user
    // decided to put there: given to a shell running as the service, a semicolon in them would be a
    // command of theirs executed as root.
    const QString runtime_dir = QString("/run/user/%1").arg(uid);
    QStringList arguments;

    arguments << "-u" << user_name << "--"
              << "env" << ("XDG_RUNTIME_DIR=" + runtime_dir)
              << ("DBUS_SESSION_BUS_ADDRESS=unix:path=" + runtime_dir + "/bus")
              << "systemd-run" << "--user" << "--collect";

    if (!display.isEmpty())
        arguments << ("--setenv=DISPLAY=" + display);
    if (!xauthority.isEmpty())
        arguments << ("--setenv=XAUTHORITY=" + xauthority);

    arguments << tool.program << tool.arguments;

    LOG(INFO) << "Starting tool" << tool.name << "in session" << session
              << "(user:" << user_name << "args:" << arguments << ")";

    QProcess process;
    process.setProgram("runuser");
    process.setArguments(arguments);
    process.start();

    // The command only asks the manager of the session to start a unit and returns as soon as it
    // has, so it is a short wait; without an end to it a session whose manager is gone would hold
    // the worker forever.
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

    QString session;
    uid_t uid = 0;
    if (!SessionUtil::activeSession(&session, &uid))
    {
        LOG(ERROR) << "No active session on seat0";
        return false;
    }

    const QString user_name = SessionUtil::userNameByUid(uid);
    const QString home_dir = homeDirectory(uid);
    if (user_name.isEmpty() || home_dir.isEmpty())
        return false;

    QStringList arguments;
    const QString terminal = scriptTerminal(&arguments);
    if (terminal.isEmpty())
    {
        LOG(ERROR) << "No terminal emulator is installed";
        return false;
    }

    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();

    // The window belongs on the desktop of the user, so the terminal is told where that desktop is:
    // an X display with the cookie of its server, a Wayland socket, or both. Which of the two it
    // uses is decided by the toolkit the terminal is built with, and a machine may have a terminal
    // that only speaks one of them.
    QString display;
    QString xauthority;
    if (SessionUtil::readX11Env(uid, session, &display, &xauthority))
    {
        environment.insert("DISPLAY", display);
        if (!xauthority.isEmpty())
            environment.insert("XAUTHORITY", xauthority);
    }

    const QString runtime_dir = QString("/run/user/%1").arg(uid);
    const QStringList sockets = QDir(runtime_dir).entryList(
        QStringList() << "wayland-[0-9]", QDir::System | QDir::NoDotAndDotDot, QDir::Name);
    if (!sockets.isEmpty())
    {
        environment.insert("XDG_RUNTIME_DIR", runtime_dir);
        environment.insert("WAYLAND_DISPLAY", sockets.first());
    }

    if (!environment.contains("DISPLAY") && !environment.contains("WAYLAND_DISPLAY"))
    {
        LOG(ERROR) << "No display found for session" << session;
        return false;
    }

    // What a script needs to know about the person the machine is being fixed for: running with the
    // rights of the service, it sees the account of the service in its own environment.
    environment.insert("SESSION_USER", user_name);
    environment.insert("SESSION_HOME", home_dir);

    // The name of the window is set by the script itself: every terminal has its own way of taking
    // it on the command line, but all of them understand the escape sequence for it. The window is
    // kept open until a key is pressed, so that what the script printed can be read; a key and not a
    // shell prompt, which would leave a terminal with the rights of the service on the desktop.
    const QString text = QString(
        "printf '\\033]0;Aspia - %1\\007'\n"
        "%2"
        "echo\n"
        "read -n 1 -s -r -p 'Press any key to close this window...'\n"
        "echo\n").arg(script.name, script.command);

    QProcess process;
    process.setProgram(terminal);
    process.setArguments(arguments << "bash" << "-c" << text);
    process.setProcessEnvironment(environment);

    LOG(INFO) << "Starting script" << script.name << "in session" << session
              << "(user:" << user_name << "terminal:" << terminal << ")";

    qint64 pid = 0;
    if (!process.startDetached(&pid))
    {
        LOG(ERROR) << "Unable to start" << terminal << ":" << process.errorString();
        return false;
    }

    return true;
}
