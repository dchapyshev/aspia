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

#include <sstream>
#include <inipp.h>

#include "base/files/file_util.h"
#include "base/logging.h"

namespace base {

namespace {

const char kSystemdPath[] = "/etc/systemd/system";
using IniFile = inipp::Ini<char>;

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
bool readUnitFile(const QString& path, IniFile* ini)
{
    QByteArray buffer;
    if (!readFile(path, &buffer))
        return false;

    std::istringstream stream(buffer.toStdString());
    ini->clear();
    ini->parse(stream);
    return true;
}

//--------------------------------------------------------------------------------------------------
bool writeUnitFile(const QString& path, IniFile ini)
{
    std::ostringstream stream;
    ini.generate(stream);

    const std::string content = stream.str();
    return writeFile(path, content);
}

//--------------------------------------------------------------------------------------------------
QString readKeyValue(const IniFile& ini, const QString& section, const QString& key)
{
    auto section_it = ini.sections.find(section.toStdString());
    if (section_it == ini.sections.end())
        return QString();

    auto key_it = section_it->second.find(key.toStdString());
    if (key_it == section_it->second.end())
        return QString();

    return QString::fromStdString(key_it->second);
}

//--------------------------------------------------------------------------------------------------
void setKeyValue(IniFile* ini, const QString& section, const QString& key, const QString& value)
{
    ini->sections[section.toStdString()][key.toStdString()] = value.toStdString();
}

//--------------------------------------------------------------------------------------------------
void removeKey(IniFile* ini, const QString& section, const QString& key)
{
    auto section_it = ini->sections.find(section.toStdString());
    if (section_it == ini->sections.end())
        return;

    section_it->second.erase(key.toStdString());
}

//--------------------------------------------------------------------------------------------------
template <typename Updater>
bool updateUnitFile(const QString& unit_name, Updater&& updater)
{
    const QString path = unitFilePath(unit_name);

    IniFile ini;
    if (!readUnitFile(path, &ini))
    {
        LOG(ERROR) << "Failed to read unit file:" << path;
        return false;
    }

    updater(&ini);

    if (!writeUnitFile(path, std::move(ini)))
    {
        LOG(ERROR) << "Failed to write unit file:" << path;
        return false;
    }

    return reloadSystemd();
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
    const QString& name, const QString& display_name, const QString& file_path)
{
    const QString service_name = unitName(name);
    const QString unit_file_path = defaultUnitFilePath(service_name);
    const QFileInfo file_info(file_path);

    IniFile ini;
    setKeyValue(&ini, "Unit", "Description", display_name);
    setKeyValue(&ini, "Unit", "After", "network-online.target");
    setKeyValue(&ini, "Unit", "Wants", "network-online.target");
    setKeyValue(&ini, "Service", "WorkingDirectory", file_info.absolutePath());
    setKeyValue(&ini, "Service", "Environment", "ASPIA_LOG_LEVEL=2");
    setKeyValue(&ini, "Service", "ExecStart", file_path);
    setKeyValue(&ini, "Service", "Restart", "always");
    setKeyValue(&ini, "Service", "RestartSec", "5s");
    setKeyValue(&ini, "Install", "WantedBy", "multi-user.target");

    if (!writeUnitFile(unit_file_path, std::move(ini)))
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
bool ServiceControllerSystemd::setAccount(const QString& username, const QString& /* password */)
{
    return updateUnitFile(unit_name_, [&](IniFile* ini)
    {
        if (username.isEmpty())
            return removeKey(ini, "Service", "User");

        setKeyValue(ini, "Service", "User", username);
    });
}

//--------------------------------------------------------------------------------------------------
QString ServiceControllerSystemd::filePath() const
{
    IniFile ini;
    if (!readUnitFile(unitFilePath(unit_name_), &ini))
        return QString();

    return readKeyValue(ini, "Service", "ExecStart");
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

} // namespace base
