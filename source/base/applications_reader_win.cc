//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/logging.h"
#include "base/win/registry.h"

namespace base {

namespace {

const char kUninstallKeyPath[] = "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall";
const char kDisplayName[] = "DisplayName";
const char kDisplayVersion[] = "DisplayVersion";
const char kPublisher[] = "Publisher";
const char kInstallDate[] = "InstallDate";
const char kInstallLocation[] = "InstallLocation";
const char kSystemComponent[] = "SystemComponent";
const char kParentKeyName[] = "ParentKeyName";

//--------------------------------------------------------------------------------------------------
bool addApplication(
    proto::system_info::Applications* message, const QString& key_name, REGSAM access)
{
    QString key_path = QString("%1\\%2").arg(kUninstallKeyPath, key_name);
    RegistryKey key;

    LONG status = key.open(HKEY_LOCAL_MACHINE, key_path, access | KEY_READ);
    if (status != ERROR_SUCCESS)
    {
        LOG(LS_ERROR) << "Unable to open registry key: "
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

    status = key.readValue(kInstallDate, &value);
    if (status == ERROR_SUCCESS)
        item->set_install_date(value.toStdString());

    status = key.readValue(kInstallLocation, &value);
    if (status == ERROR_SUCCESS)
        item->set_install_location(value.toStdString());

    return true;
}

} // namespace

//--------------------------------------------------------------------------------------------------
void readApplicationsInformation(proto::system_info::Applications* applications)
{
    RegistryKeyIterator machine_key_iterator(HKEY_LOCAL_MACHINE, kUninstallKeyPath);

    while (machine_key_iterator.valid())
    {
        addApplication(applications, machine_key_iterator.name(), 0);
        ++machine_key_iterator;
    }

    RegistryKeyIterator user_key_iterator(HKEY_CURRENT_USER, kUninstallKeyPath);

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
        RegistryKeyIterator machine64_key_iterator(HKEY_LOCAL_MACHINE,
                                                   kUninstallKeyPath,
                                                   KEY_WOW64_64KEY);

        while (machine64_key_iterator.valid())
        {
            addApplication(applications, machine64_key_iterator.name(), KEY_WOW64_64KEY);
            ++machine64_key_iterator;
        }

        RegistryKeyIterator user64_key_iterator(HKEY_CURRENT_USER,
                                                kUninstallKeyPath,
                                                KEY_WOW64_64KEY);

        while (user64_key_iterator.valid())
        {
            addApplication(applications, user64_key_iterator.name(), KEY_WOW64_64KEY);
            ++user64_key_iterator;
        }
    }

#elif defined(Q_PROCESSOR_X86_64)

    RegistryKeyIterator machine32_key_iterator(HKEY_LOCAL_MACHINE,
                                               kUninstallKeyPath,
                                               KEY_WOW64_32KEY);

    while (machine32_key_iterator.valid())
    {
        addApplication(applications, machine32_key_iterator.name(), KEY_WOW64_32KEY);
        ++machine32_key_iterator;
    }

    RegistryKeyIterator user32_key_iterator(HKEY_CURRENT_USER,
                                            kUninstallKeyPath,
                                            KEY_WOW64_32KEY);

    while (user32_key_iterator.valid())
    {
        addApplication(applications, user32_key_iterator.name(), KEY_WOW64_32KEY);
        ++user32_key_iterator;
    }

#else
#error Unknown Architecture
#endif
}

} // namespace base
