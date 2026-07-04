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

#include "base/applications_reader.h"

#include <QFile>
#include <QProcess>

#include "base/logging.h"
#include "proto/system_info.h"

#if defined(Q_OS_WINDOWS)
#include "base/win/registry.h"
#endif // defined(Q_OS_WINDOWS)

namespace {

#if defined(Q_OS_WINDOWS)
const char kUninstallKeyPath[] = "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall";
const char kDisplayName[] = "DisplayName";
const char kDisplayVersion[] = "DisplayVersion";
const char kPublisher[] = "Publisher";
const char kSystemComponent[] = "SystemComponent";
const char kParentKeyName[] = "ParentKeyName";

//--------------------------------------------------------------------------------------------------
bool addApplication(
    proto::system_info::Applications* message, const QString& key_name, REGSAM access)
{
    QString key_path = QString("%1\\%2").arg(kUninstallKeyPath, key_name);
    RegKey key;

    LONG status = key.open(HKEY_LOCAL_MACHINE, key_path, access | KEY_READ);
    if (status != ERROR_SUCCESS)
    {
        LOG(ERROR) << "Unable to open registry key:"
                   << SystemError(static_cast<DWORD>(status)).toString();
        return false;
    }

    DWORD system_component = 0;

    status = key.readValueDW(kSystemComponent, &system_component);
    if (status == ERROR_SUCCESS && system_component == 1)
        return false;

    QString value;

    status = key.readValue(kParentKeyName, &value);
    if (status == ERROR_SUCCESS)
        return false;

    status = key.readValue(kDisplayName, &value);
    if (status != ERROR_SUCCESS)
        return false;

    proto::system_info::Applications::Application* item = message->add_application();

    item->set_name(value.toStdString());

    status = key.readValue(kDisplayVersion, &value);
    if (status == ERROR_SUCCESS)
        item->set_version(value.toStdString());

    status = key.readValue(kPublisher, &value);
    if (status == ERROR_SUCCESS)
        item->set_publisher(value.toStdString());

    return true;
}
#endif // defined(Q_OS_WINDOWS)

#if defined(Q_OS_LINUX)
//--------------------------------------------------------------------------------------------------
// Reads installed packages from the dpkg database (Debian/Ubuntu). Returns false if the database is
// absent or empty - dpkg may be installed on an rpm-based distribution (only to build .deb packages),
// leaving an empty status file, in which case the caller should fall back to the rpm database.
bool readDpkgPackages(proto::system_info::Applications* applications)
{
    QFile file("/var/lib/dpkg/status");
    if (!file.open(QIODevice::ReadOnly))
        return false;

    const int initial_count = applications->application_size();

    QByteArray name;
    QByteArray version;
    QByteArray maintainer;
    bool installed = false;

    auto flush = [&]()
    {
        if (installed && !name.isEmpty())
        {
            proto::system_info::Applications::Application* item = applications->add_application();
            item->set_name(name.toStdString());
            if (!version.isEmpty())
                item->set_version(version.toStdString());
            if (!maintainer.isEmpty())
                item->set_publisher(maintainer.toStdString());
        }

        name.clear();
        version.clear();
        maintainer.clear();
        installed = false;
    };

    // The status file is a sequence of stanzas separated by blank lines; one stanza per package.
    QByteArray line;
    while (!(line = file.readLine()).isEmpty())
    {
        if (line.trimmed().isEmpty())
        {
            flush();
            continue;
        }

        // Continuation lines of multi-line fields (Description) start with whitespace.
        if (line.at(0) == ' ' || line.at(0) == '\t')
            continue;

        int colon = line.indexOf(':');
        if (colon < 0)
            continue;

        const QByteArray key = line.left(colon).trimmed();
        const QByteArray value = line.mid(colon + 1).trimmed();

        if (key == "Package")
            name = value;
        else if (key == "Version")
            version = value;
        else if (key == "Maintainer")
            maintainer = value;
        else if (key == "Status")
            installed = value.contains("installed"); // e.g. "install ok installed".
    }

    flush();
    return applications->application_size() > initial_count;
}

//--------------------------------------------------------------------------------------------------
// Reads installed packages from the rpm database (RedHat/Fedora/SUSE) via the rpm tool.
void readRpmPackages(proto::system_info::Applications* applications)
{
    QProcess process;
    process.start("rpm", QStringList()
        << "-qa" << "--qf" << "%{NAME}\\t%{VERSION}-%{RELEASE}\\t%{VENDOR}\\n");
    if (!process.waitForStarted() || !process.waitForFinished())
        return;

    const QList<QByteArray> lines = process.readAllStandardOutput().split('\n');
    for (const QByteArray& line : lines)
    {
        if (line.isEmpty())
            continue;

        const QList<QByteArray> parts = line.split('\t');
        if (parts.isEmpty() || parts.at(0).isEmpty())
            continue;

        proto::system_info::Applications::Application* item = applications->add_application();
        item->set_name(parts.at(0).toStdString());

        if (parts.size() > 1 && !parts.at(1).isEmpty())
            item->set_version(parts.at(1).toStdString());

        if (parts.size() > 2 && !parts.at(2).isEmpty() && parts.at(2) != "(none)")
            item->set_publisher(parts.at(2).toStdString());
    }
}
#endif // defined(Q_OS_LINUX)

} // namespace

//--------------------------------------------------------------------------------------------------
void readApplicationsInformation(proto::system_info::Applications* applications)
{
#if defined(Q_OS_WINDOWS)
    RegKeyIterator machine_key_iterator(HKEY_LOCAL_MACHINE, kUninstallKeyPath);
    while (machine_key_iterator.valid())
    {
        addApplication(applications, machine_key_iterator.name(), 0);
        ++machine_key_iterator;
    }

    RegKeyIterator user_key_iterator(HKEY_CURRENT_USER, kUninstallKeyPath);
    while (user_key_iterator.valid())
    {
        addApplication(applications, user_key_iterator.name(), 0);
        ++user_key_iterator;
    }

#if defined(Q_PROCESSOR_X86_32)
    BOOL is_wow64;

    // If the x86 application is running in a x64 system.
    if (IsWow64Process(GetCurrentProcess(), &is_wow64) && is_wow64)
    {
        RegKeyIterator machine64_key_iterator(HKEY_LOCAL_MACHINE, kUninstallKeyPath, KEY_WOW64_64KEY);
        while (machine64_key_iterator.valid())
        {
            addApplication(applications, machine64_key_iterator.name(), KEY_WOW64_64KEY);
            ++machine64_key_iterator;
        }

        RegKeyIterator user64_key_iterator(HKEY_CURRENT_USER, kUninstallKeyPath, KEY_WOW64_64KEY);
        while (user64_key_iterator.valid())
        {
            addApplication(applications, user64_key_iterator.name(), KEY_WOW64_64KEY);
            ++user64_key_iterator;
        }
    }
#elif defined(Q_PROCESSOR_X86_64)
    RegKeyIterator machine32_key_iterator(HKEY_LOCAL_MACHINE, kUninstallKeyPath, KEY_WOW64_32KEY);
    while (machine32_key_iterator.valid())
    {
        addApplication(applications, machine32_key_iterator.name(), KEY_WOW64_32KEY);
        ++machine32_key_iterator;
    }

    RegKeyIterator user32_key_iterator(HKEY_CURRENT_USER, kUninstallKeyPath, KEY_WOW64_32KEY);
    while (user32_key_iterator.valid())
    {
        addApplication(applications, user32_key_iterator.name(), KEY_WOW64_32KEY);
        ++user32_key_iterator;
    }
#else
#error Unknown Architecture
#endif
#elif defined(Q_OS_LINUX)
    if (!readDpkgPackages(applications))
        readRpmPackages(applications);
#else
    Q_UNUSED(applications)
    NOTIMPLEMENTED();
#endif
}
