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

#include "base/service_controller_launchd.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include <CoreFoundation/CoreFoundation.h>

#include "base/logging.h"

namespace {

// System-wide daemons managed by launchd live here and are started at boot as root.
const char kLaunchDaemonsPath[] = "/Library/LaunchDaemons";

//--------------------------------------------------------------------------------------------------
// Derives a stable reverse-DNS launchd label from the service name (e.g. "aspia-host-service" ->
// "org.aspia.host-service"). open()/install()/remove() all funnel through this, so the label and
// therefore the plist path are identical for a given name.
QString serviceLabel(const QString& name)
{
    QString suffix = name;
    if (suffix.startsWith("aspia-"))
        suffix = suffix.mid(6);

    return "org.aspia." + suffix;
}

//--------------------------------------------------------------------------------------------------
QString plistPath(const QString& label)
{
    return QDir(QString::fromUtf8(kLaunchDaemonsPath)).filePath(label + ".plist");
}

//--------------------------------------------------------------------------------------------------
// The application bundle identifier, used to attribute the daemon to the app in Login Items so it is
// listed under the app name instead of the signing team name.
QString mainBundleIdentifier()
{
    CFBundleRef bundle = CFBundleGetMainBundle();
    if (!bundle)
        return QString();

    CFStringRef identifier = CFBundleGetIdentifier(bundle);
    if (!identifier)
        return QString();

    return QString::fromCFString(identifier);
}

//--------------------------------------------------------------------------------------------------
void writeKeyString(QXmlStreamWriter& writer, const QString& key, const QString& value)
{
    writer.writeTextElement("key", key);
    writer.writeTextElement("string", value);
}

//--------------------------------------------------------------------------------------------------
void writeKeyBool(QXmlStreamWriter& writer, const QString& key, bool value)
{
    writer.writeTextElement("key", key);
    writer.writeEmptyElement(value ? "true" : "false");
}

//--------------------------------------------------------------------------------------------------
void writeKeyInt(QXmlStreamWriter& writer, const QString& key, int value)
{
    writer.writeTextElement("key", key);
    writer.writeTextElement("integer", QString::number(value));
}

//--------------------------------------------------------------------------------------------------
// Builds a system-daemon plist. launchd starts the executable with no arguments, which is the mode
// in which the service runs its main loop (install/remove/start/stop are handled before this point).
// An empty |username| leaves the daemon running under the default account (root).
QByteArray generatePlist(const QString& label, const QString& file_path,
                         const QStringList& arguments, const QString& username)
{
    const QFileInfo file_info(file_path);

    QByteArray data;
    QXmlStreamWriter writer(&data);
    writer.setAutoFormatting(true);

    writer.writeStartDocument("1.0");
    writer.writeDTD("<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
                    "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">");
    writer.writeStartElement("plist");
    writer.writeAttribute("version", "1.0");
    writer.writeStartElement("dict");

    writeKeyString(writer, "Label", label);

    // Associate the daemon with the application bundle so Login Items lists it under the app name
    // ("Aspia Host") instead of the Developer ID team name. Requires the daemon and the app to be
    // signed by the same team.
    const QString bundle_id = mainBundleIdentifier();
    if (!bundle_id.isEmpty())
    {
        writer.writeTextElement("key", "AssociatedBundleIdentifiers");
        writer.writeStartElement("array");
        writer.writeTextElement("string", bundle_id);
        writer.writeEndElement(); // array
    }

    writer.writeTextElement("key", "ProgramArguments");
    writer.writeStartElement("array");
    writer.writeTextElement("string", file_path);
    for (const QString& argument : arguments)
        writer.writeTextElement("string", argument);
    writer.writeEndElement(); // array

    writeKeyString(writer, "WorkingDirectory", file_info.absolutePath());

    if (!username.isEmpty())
    {
        writeKeyString(writer, "UserName", username);
        writeKeyString(writer, "GroupName", username);
    }

    writer.writeTextElement("key", "EnvironmentVariables");
    writer.writeStartElement("dict");
    writeKeyString(writer, "ASPIA_LOG_LEVEL", "2");
    writer.writeEndElement(); // dict

    // Start at boot and keep the daemon alive, restarting it if it exits (mirrors Restart=always).
    writeKeyBool(writer, "RunAtLoad", true);
    writeKeyBool(writer, "KeepAlive", true);

    // Run at interactive contention level. Without this launchd treats the daemon as a background
    // job, so macOS applies aggressive timer coalescing (~8 Hz) and I/O throttling to it and the
    // desktop-agent process it spawns - which caps the remote-desktop frame rate far below the
    // configured FPS. Interactive gives it the same scheduling as user-facing apps.
    writeKeyString(writer, "ProcessType", "Interactive");

    // Nudge the scheduler priority up a little so the daemon keeps relaying frames promptly under
    // system load. A mild value (not the -20 maximum) to avoid starving the rest of the system.
    writeKeyInt(writer, "Nice", -10);

    writer.writeEndElement(); // dict
    writer.writeEndElement(); // plist
    writer.writeEndDocument();

    return data;
}

//--------------------------------------------------------------------------------------------------
bool writePlist(const QString& path, const QByteArray& content)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        LOG(ERROR) << "Failed to open plist for writing:" << path << file.errorString();
        return false;
    }

    if (file.write(content) != content.size())
    {
        LOG(ERROR) << "Failed to write plist:" << path << file.errorString();
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
// Returns the executable path stored in ProgramArguments (the first <string> of the array).
QString readProgramPath(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return QString();

    QXmlStreamReader reader(&file);
    while (!reader.atEnd())
    {
        if (reader.readNext() != QXmlStreamReader::StartElement)
            continue;

        if (reader.name() != QLatin1String("key") ||
            reader.readElementText() != QLatin1String("ProgramArguments"))
        {
            continue;
        }

        // The array follows the key; its first <string> is the executable path.
        while (!reader.atEnd())
        {
            if (reader.readNext() == QXmlStreamReader::StartElement &&
                reader.name() == QLatin1String("string"))
            {
                return reader.readElementText();
            }
        }
    }

    return QString();
}

//--------------------------------------------------------------------------------------------------
// Returns the command-line arguments stored in ProgramArguments (every <string> of the array after
// the first, which is the executable path).
QStringList readProgramArguments(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return QStringList();

    QXmlStreamReader reader(&file);
    while (!reader.atEnd())
    {
        if (reader.readNext() != QXmlStreamReader::StartElement)
            continue;

        if (reader.name() != QLatin1String("key") ||
            reader.readElementText() != QLatin1String("ProgramArguments"))
        {
            continue;
        }

        QStringList strings;
        while (!reader.atEnd())
        {
            const QXmlStreamReader::TokenType token = reader.readNext();
            if (token == QXmlStreamReader::StartElement &&
                reader.name() == QLatin1String("string"))
            {
                strings.append(reader.readElementText());
            }
            else if (token == QXmlStreamReader::EndElement &&
                     reader.name() == QLatin1String("array"))
            {
                break;
            }
        }

        if (!strings.isEmpty())
            strings.removeFirst(); // drop the executable path
        return strings;
    }

    return QStringList();
}

//--------------------------------------------------------------------------------------------------
bool runProcess(const QString& program, const QStringList& arguments, QByteArray* output = nullptr)
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

    if (output)
        *output = process.readAllStandardOutput();

    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0)
    {
        const QByteArray standard_error = process.readAllStandardError().trimmed();
        LOG(ERROR) << program << "failed:" << arguments << standard_error;
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool runLaunchctl(const QStringList& arguments, QByteArray* output = nullptr)
{
    return runProcess("launchctl", arguments, output);
}

} // namespace

//--------------------------------------------------------------------------------------------------
ServiceControllerLaunchd::ServiceControllerLaunchd()
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
ServiceControllerLaunchd::ServiceControllerLaunchd(const QString& label)
    : label_(label)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
ServiceControllerLaunchd::~ServiceControllerLaunchd() = default;

//--------------------------------------------------------------------------------------------------
// static
std::unique_ptr<ServiceController> ServiceControllerLaunchd::open(const QString& name)
{
    const QString label = serviceLabel(name);
    if (!QFileInfo::exists(plistPath(label)))
        return nullptr;

    return std::unique_ptr<ServiceController>(new ServiceControllerLaunchd(label));
}

//--------------------------------------------------------------------------------------------------
// static
std::unique_ptr<ServiceController> ServiceControllerLaunchd::install(
    const QString& name, const QString& /* display_name */, const QString& file_path,
    const QStringList& arguments)
{
    const QString label = serviceLabel(name);
    const QString path = plistPath(label);

    if (!writePlist(path, generatePlist(label, file_path, arguments, QString())))
        return nullptr;

    // Load and enable the daemon. RunAtLoad in the plist also starts it immediately.
    if (!runLaunchctl({ "load", "-w", path }))
    {
        QFile::remove(path);
        return nullptr;
    }

    return open(name);
}

//--------------------------------------------------------------------------------------------------
// static
bool ServiceControllerLaunchd::remove(const QString& name)
{
    const QString label = serviceLabel(name);
    const QString path = plistPath(label);

    if (!QFileInfo::exists(path))
        return false;

    // Unload and disable the daemon. Ignore the result: the job may already be unloaded, but the
    // plist still has to go so isInstalled() reports it removed.
    runLaunchctl({ "unload", "-w", path });

    if (!QFile::remove(path))
    {
        LOG(ERROR) << "Failed to remove plist:" << path;
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
// static
bool ServiceControllerLaunchd::isInstalled(const QString& name)
{
    return QFileInfo::exists(plistPath(serviceLabel(name)));
}

//--------------------------------------------------------------------------------------------------
// static
bool ServiceControllerLaunchd::isRunning(const QString& name)
{
    QByteArray output;
    // "launchctl list <label>" exits non-zero when the job is not loaded. When it is loaded the
    // output is a dictionary that contains a "PID" key only while a process is actually running.
    if (!runLaunchctl({ "list", serviceLabel(name) }, &output))
        return false;

    return output.contains("\"PID\"");
}

//--------------------------------------------------------------------------------------------------
bool ServiceControllerLaunchd::setDescription(const QString& /* description */)
{
    // launchd has no per-daemon description field.
    return true;
}

//--------------------------------------------------------------------------------------------------
QString ServiceControllerLaunchd::description() const
{
    return QString();
}

//--------------------------------------------------------------------------------------------------
bool ServiceControllerLaunchd::setDependencies(const QStringList& /* dependencies */)
{
    return true;
}

//--------------------------------------------------------------------------------------------------
QStringList ServiceControllerLaunchd::dependencies() const
{
    return QStringList();
}

//--------------------------------------------------------------------------------------------------
bool ServiceControllerLaunchd::setAccount(const QString& username, const QString& /* password */,
                                          const QStringList& paths)
{
    const QString path = plistPath(label_);

    // Regenerating the plist requires the executable path it already points at.
    const QString program = readProgramPath(path);
    if (program.isEmpty())
    {
        LOG(ERROR) << "Failed to read ProgramArguments from plist:" << path;
        return false;
    }

    if (!username.isEmpty())
    {
        // Give the account access to the directories it reads and writes.
        for (const QString& dir : paths)
        {
            QDir().mkpath(dir);

            if (!runProcess("chown", { "-R", username, dir }))
            {
                LOG(ERROR) << "Failed to change owner of" << dir;
                return false;
            }
        }
    }

    // Rewrite the plist with (or without) the account, then reload so launchd picks up the change.
    // An empty |username| restores the default account (root). Preserve the program arguments the
    // service was installed with (e.g. "--service").
    if (!writePlist(path, generatePlist(label_, program, readProgramArguments(path), username)))
        return false;

    runLaunchctl({ "unload", "-w", path });
    return runLaunchctl({ "load", "-w", path });
}

//--------------------------------------------------------------------------------------------------
QString ServiceControllerLaunchd::filePath() const
{
    return readProgramPath(plistPath(label_));
}

//--------------------------------------------------------------------------------------------------
bool ServiceControllerLaunchd::isRunning() const
{
    QByteArray output;
    if (!runLaunchctl({ "list", label_ }, &output))
        return false;

    return output.contains("\"PID\"");
}

//--------------------------------------------------------------------------------------------------
bool ServiceControllerLaunchd::start()
{
    return runLaunchctl({ "start", label_ });
}

//--------------------------------------------------------------------------------------------------
bool ServiceControllerLaunchd::stop()
{
    return runLaunchctl({ "stop", label_ });
}
