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

#include "base/service_controller_systemd.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>

#include <pwd.h>

#include "base/ini_file.h"
#include "base/logging.h"

namespace {

using namespace Qt::StringLiterals;

const char kSystemdPath[] = "/etc/systemd/system";

// The names come from systemd.unit(5) as they are, mixed case included.
const QByteArray kUnitSection = "Unit"_ba;
const QByteArray kServiceSection = "Service"_ba;
const QByteArray kInstallSection = "Install"_ba;

// Sandboxing directives applied together with the low-privilege account. See systemd.exec(5). Both
// the router and the relay are network daemons that only need their own directories, so the same
// set fits both. An empty CapabilityBoundingSet drops all capabilities.
const struct
{
    const char* key;
    const char* value;
} kSandboxing[] = {
    { "NoNewPrivileges",         "yes" },
    { "ProtectSystem",           "strict" },
    { "ProtectHome",             "yes" },
    { "PrivateTmp",              "yes" },
    { "PrivateDevices",          "yes" },
    { "ProtectKernelTunables",   "yes" },
    { "ProtectControlGroups",    "yes" },
    { "RestrictAddressFamilies", "AF_INET AF_INET6" },
    { "RestrictNamespaces",      "yes" },
    { "CapabilityBoundingSet",   "" },
};

//--------------------------------------------------------------------------------------------------
QString unitName(const QString& name)
{
    if (name.endsWith(".service"))
        return name;

    return name + ".service";
}

//--------------------------------------------------------------------------------------------------
QString defaultUnitFilePath(const QString& unit_name)
{
    return QDir(QString::fromUtf8(kSystemdPath)).filePath(unit_name);
}

//--------------------------------------------------------------------------------------------------
bool runSystemctl(const QStringList& arguments, QByteArray* output = nullptr)
{
    QProcess process;
    process.start("systemctl", arguments);

    if (!process.waitForStarted())
    {
        PLOG(ERROR) << "Failed to start systemctl";
        return false;
    }

    if (!process.waitForFinished())
    {
        LOG(ERROR) << "systemctl did not finish:" << arguments;
        return false;
    }

    if (output)
        *output = process.readAllStandardOutput();

    if (process.exitStatus() != QProcess::NormalExit)
    {
        LOG(ERROR) << "systemctl terminated abnormally:" << arguments;
        return false;
    }

    if (process.exitCode() != 0)
    {
        const QByteArray standard_error = process.readAllStandardError().trimmed();
        LOG(ERROR) << "systemctl failed:" << arguments << standard_error;
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
QString systemctlShowValue(const QString& unit_name, const QString& property)
{
    QByteArray output;
    if (!runSystemctl({ "show", "--property", property, "--value", unit_name }, &output))
        return QString();

    return QString::fromUtf8(output).trimmed();
}

//--------------------------------------------------------------------------------------------------
bool isUnitLoaded(const QString& unit_name)
{
    const QString load_state = systemctlShowValue(unit_name, "LoadState");
    return !load_state.isEmpty() && load_state != "not-found" && load_state != "error";
}

//--------------------------------------------------------------------------------------------------
QString unitFilePath(const QString& unit_name)
{
    const QString fragment_path = systemctlShowValue(unit_name, "FragmentPath");
    if (!fragment_path.isEmpty())
        return fragment_path;

    return defaultUnitFilePath(unit_name);
}

//--------------------------------------------------------------------------------------------------
bool reloadSystemd()
{
    return runSystemctl({ "daemon-reload" });
}

//--------------------------------------------------------------------------------------------------
template <typename Updater>
bool updateUnitFile(const QString& unit_name, Updater&& updater)
{
    const QString path = unitFilePath(unit_name);

    // A unit that is not there must not spring into existence as a bare fragment.
    if (!QFileInfo::exists(path))
    {
        LOG(ERROR) << "Unit file does not exist:" << path;
        return false;
    }

    IniFile ini(path);
    if (ini.hasErrors())
    {
        LOG(ERROR) << "Failed to read unit file:" << path;
        return false;
    }

    updater(&ini);

    if (!ini.sync())
    {
        LOG(ERROR) << "Failed to write unit file:" << path;
        return false;
    }

    return reloadSystemd();
}

//--------------------------------------------------------------------------------------------------
bool runProcess(const QString& program, const QStringList& arguments)
{
    QProcess process;
    process.start(program, arguments);

    if (!process.waitForStarted())
    {
        PLOG(ERROR) << "Failed to start" << program;
        return false;
    }

    if (!process.waitForFinished())
    {
        LOG(ERROR) << program << "did not finish:" << arguments;
        return false;
    }

    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0)
    {
        const QByteArray standard_error = process.readAllStandardError().trimmed();
        LOG(ERROR) << program << "failed:" << arguments << standard_error;
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool ensureUserExists(const QString& user)
{
    if (getpwnam(user.toLocal8Bit().constData()))
        return true;

    return runProcess("useradd", { "--system", "--user-group", "--no-create-home",
                                   "--shell", "/usr/sbin/nologin", user });
}

} // namespace

//--------------------------------------------------------------------------------------------------
ServiceControllerSystemd::ServiceControllerSystemd()
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
ServiceControllerSystemd::ServiceControllerSystemd(const QString& unit_name)
    : unit_name_(unit_name)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
ServiceControllerSystemd::~ServiceControllerSystemd() = default;

//--------------------------------------------------------------------------------------------------
// static
std::unique_ptr<ServiceController> ServiceControllerSystemd::open(const QString& name)
{
    const QString service_name = unitName(name);
    if (!isUnitLoaded(service_name))
        return nullptr;

    return std::unique_ptr<ServiceController>(new ServiceControllerSystemd(service_name));
}

//--------------------------------------------------------------------------------------------------
// static
std::unique_ptr<ServiceController> ServiceControllerSystemd::install(
    const QString& name, const QString& display_name, const QString& file_path,
    const QStringList& arguments)
{
    const QString service_name = unitName(name);
    const QString unit_file_path = defaultUnitFilePath(service_name);
    const QFileInfo file_info(file_path);

    QString exec_start = file_path;
    if (!arguments.isEmpty())
        exec_start += ' ' + arguments.join(' ');

    // The installation writes the unit anew, so nothing of an older unit may seep into it.
    if (QFileInfo::exists(unit_file_path) && !QFile::remove(unit_file_path))
    {
        LOG(ERROR) << "Failed to remove unit file:" << unit_file_path;
        return nullptr;
    }

    IniFile ini(unit_file_path);
    ini.setStringValue(kUnitSection, "Description", display_name);
    ini.setStringValue(kUnitSection, "After", "network-online.target");
    ini.setStringValue(kUnitSection, "Wants", "network-online.target");
    ini.setStringValue(kServiceSection, "WorkingDirectory", file_info.absolutePath());
    ini.setStringValue(kServiceSection, "Environment", "ASPIA_LOG_LEVEL=2");
    ini.setStringValue(kServiceSection, "ExecStart", exec_start);
    ini.setStringValue(kServiceSection, "Restart", "always");
    ini.setStringValue(kServiceSection, "RestartSec", "5s");
    ini.setStringValue(kInstallSection, "WantedBy", "multi-user.target");

    if (!ini.sync())
    {
        LOG(ERROR) << "Failed to write unit file:" << unit_file_path;
        return nullptr;
    }

    if (!reloadSystemd())
        return nullptr;

    if (!runSystemctl({ "enable", service_name }))
        return nullptr;

    return open(service_name);
}

//--------------------------------------------------------------------------------------------------
// static
bool ServiceControllerSystemd::remove(const QString& name)
{
    const QString service_name = unitName(name);
    if (!isInstalled(service_name))
        return false;

    const QString path = unitFilePath(service_name);

    if (!runSystemctl({ "disable", service_name }))
        return false;

    if (QFileInfo::exists(path) && path.startsWith(QString::fromUtf8(kSystemdPath)))
    {
        if (!QFile::remove(path))
        {
            LOG(ERROR) << "Failed to remove unit file:" << path;
            return false;
        }
    }

    return reloadSystemd();
}

//--------------------------------------------------------------------------------------------------
// static
bool ServiceControllerSystemd::isInstalled(const QString& name)
{
    const QString service_name = unitName(name);
    const QString path = unitFilePath(service_name);

    return isUnitLoaded(service_name) || QFileInfo::exists(path);
}

//--------------------------------------------------------------------------------------------------
// static
bool ServiceControllerSystemd::isRunning(const QString& name)
{
    QProcess process;
    process.start("systemctl", { "is-active", "--quiet", unitName(name) });

    if (!process.waitForStarted())
    {
        PLOG(ERROR) << "Failed to start systemctl";
        return false;
    }

    if (!process.waitForFinished())
    {
        LOG(ERROR) << "systemctl is-active did not finish";
        return false;
    }

    if (process.exitStatus() != QProcess::NormalExit)
    {
        LOG(ERROR) << "systemctl is-active terminated abnormally";
        return false;
    }

    return process.exitCode() == 0;
}

//--------------------------------------------------------------------------------------------------
bool ServiceControllerSystemd::setDescription(const QString& /* description */)
{
    return true;
}

//--------------------------------------------------------------------------------------------------
QString ServiceControllerSystemd::description() const
{
    return QString();
}

//--------------------------------------------------------------------------------------------------
bool ServiceControllerSystemd::setDependencies(const QStringList& /* dependencies */)
{
    return true;
}

//--------------------------------------------------------------------------------------------------
QStringList ServiceControllerSystemd::dependencies() const
{
    return QStringList();
}

//--------------------------------------------------------------------------------------------------
bool ServiceControllerSystemd::setAccount(const QString& username, const QString& /* password */,
                                          const QStringList& paths)
{
    // Provision the account before pointing the unit at it, so a failure here leaves the service on
    // its previous account.
    if (!username.isEmpty())
    {
        if (!ensureUserExists(username))
        {
            LOG(ERROR) << "Failed to create system user" << username;
            return false;
        }

        // Own the directories so the service can read its configuration and write its state,
        // including any files a prior create-config left owned by root.
        for (const QString& path : paths)
        {
            QDir().mkpath(path);

            if (!runProcess("chown", { "-R", QString("%1:%1").arg(username), path }))
            {
                LOG(ERROR) << "Failed to change owner of" << path;
                return false;
            }
        }
    }

    return updateUnitFile(unit_name_, [&](IniFile* ini)
    {
        if (username.isEmpty())
        {
            // Restore the default account and drop the sandboxing along with it.
            ini->removeValue(kServiceSection, "User");
            ini->removeValue(kServiceSection, "Group");
            ini->removeValue(kServiceSection, "ReadWritePaths");

            for (const auto& directive : kSandboxing)
                ini->removeValue(kServiceSection, directive.key);
            return;
        }

        ini->setStringValue(kServiceSection, "User", username);
        ini->setStringValue(kServiceSection, "Group", username);

        // Allow writes only to the service directories; the rest of the file system is read-only
        // under ProtectSystem=strict.
        if (!paths.isEmpty())
            ini->setStringValue(kServiceSection, "ReadWritePaths", paths.join(' '));

        for (const auto& directive : kSandboxing)
            ini->setStringValue(kServiceSection, directive.key, directive.value);
    });
}

//--------------------------------------------------------------------------------------------------
QString ServiceControllerSystemd::filePath() const
{
    const IniFile ini(unitFilePath(unit_name_));
    return ini.stringValue(kServiceSection, "ExecStart");
}

//--------------------------------------------------------------------------------------------------
bool ServiceControllerSystemd::isRunning() const
{
    return isRunning(unit_name_);
}

//--------------------------------------------------------------------------------------------------
bool ServiceControllerSystemd::start()
{
    return runSystemctl({ "start", unit_name_ });
}

//--------------------------------------------------------------------------------------------------
bool ServiceControllerSystemd::stop()
{
    return runSystemctl({ "stop", unit_name_ });
}
