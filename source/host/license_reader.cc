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

#include "host/license_reader.h"

#include <QByteArray>
#include <QFile>

#include <string_view>

#include "base/logging.h"
#include "proto/system_info.h"

#if defined(Q_OS_WINDOWS)
#include "base/win/registry.h"
#include "base/win/windows_version.h"
#endif // defined(Q_OS_WINDOWS)

namespace {

#if defined(Q_OS_WINDOWS) || defined(Q_OS_LINUX)
//--------------------------------------------------------------------------------------------------
// Returns the contents of the ACPI "MSDM" table. The table is present only if the manufacturer has
// embedded a product key into the firmware.
QByteArray msdmTable()
{
#if defined(Q_OS_WINDOWS)
    // Table identifiers are passed to the API as little-endian DWORDs, so the signature "MSDM" is
    // written in reverse.
    static const DWORD kAcpiProvider = 'ACPI';
    static const DWORD kMsdmTable = 'MDSM';

    UINT buffer_size = GetSystemFirmwareTable(kAcpiProvider, kMsdmTable, nullptr, 0);
    if (!buffer_size)
        return QByteArray();

    QByteArray buffer;
    buffer.resize(static_cast<qsizetype>(buffer_size));

    if (!GetSystemFirmwareTable(kAcpiProvider, kMsdmTable, buffer.data(), buffer_size))
    {
        PLOG(ERROR) << "GetSystemFirmwareTable failed";
        return QByteArray();
    }

    return buffer;
#else
    static const char kMsdmTablePath[] = "/sys/firmware/acpi/tables/MSDM";

    // The table is readable by root only.
    QFile file(kMsdmTablePath);
    if (!file.open(QIODevice::ReadOnly))
    {
        LOG(INFO) << "Unable to open" << kMsdmTablePath << file.errorString();
        return QByteArray();
    }

    return file.readAll();
#endif
}

//--------------------------------------------------------------------------------------------------
// The product key embedded into the firmware by the computer manufacturer. It stays there even if
// another operating system is installed.
void addOemProduct(proto::system_info::Licenses* message)
{
    // Microsoft Software Licensing Table (ACPI "MSDM"). Standard ACPI header followed by the
    // software licensing data structure with the product key.
#pragma pack(push, 1)
    struct MsdmTable
    {
        quint8 acpi_header[36];
        quint32 version;
        quint32 reserved;
        quint32 data_type;
        quint32 data_reserved;
        quint32 data_length;
        char data[1];
    };
#pragma pack(pop)

    static const quint32 kProductKeyDataType = 1;
    static const quint32 kProductKeyLength = 29;

    QByteArray table_data = msdmTable();

    if (table_data.size() < static_cast<qsizetype>(offsetof(MsdmTable, data) + kProductKeyLength))
    {
        LOG(INFO) << "No MSDM table in firmware:" << table_data.size();
        return;
    }

    const MsdmTable* table = reinterpret_cast<const MsdmTable*>(table_data.constData());

    if (table->data_type != kProductKeyDataType || table->data_length != kProductKeyLength)
    {
        LOG(ERROR) << "Unexpected MSDM table contents:" << table->data_type << table->data_length;
        return;
    }

    std::string_view product_key(table->data, table->data_length);

    for (const char ch : product_key)
    {
        if ((ch < 'A' || ch > 'Z') && (ch < '0' || ch > '9') && ch != '-')
        {
            LOG(ERROR) << "Invalid characters in OEM product key";
            return;
        }
    }

    proto::system_info::Licenses::License* item = message->add_license();

    item->set_product_name("Windows (OEM)");

    proto::system_info::Licenses::License::Field* field = item->add_field();

    field->set_type(proto::system_info::Licenses::License::Field::TYPE_PRODUCT_KEY);
    field->set_value(product_key);
}
#endif // defined(Q_OS_WINDOWS) || defined(Q_OS_LINUX)

#if defined(Q_OS_WINDOWS)
//--------------------------------------------------------------------------------------------------
std::string digitalProductIdToString(quint8* product_id, size_t product_id_size)
{
    constexpr char kKeyMap[] = "BCDFGHJKMPQRTVWXY2346789";
    constexpr int kKeyMapSize = 24;
    constexpr int kStartIndex = 52;
    constexpr int kDecodeLength = 25;
    constexpr int kDecodeStringLength = 15;
    constexpr int kGroupLength = 5;

    if (product_id_size < kStartIndex + kDecodeLength)
        return std::string();

    // The keys starting with Windows 8 / Office 2013 can contain the symbol N.
    int containsN = (product_id[kStartIndex + 14] >> 3) & 1;
    product_id[kStartIndex + 14] =
        static_cast<quint8>((product_id[kStartIndex + 14] & 0xF7) | ((containsN & 2) << 2));

    std::string key;

    for (int i = kDecodeLength - 1; i >= 0; --i)
    {
        int key_map_index = 0;

        for (int j = kDecodeStringLength - 1; j >= 0; --j)
        {
            key_map_index = (key_map_index << 8) | product_id[kStartIndex + j];
            product_id[kStartIndex + j] = static_cast<quint8>(key_map_index / kKeyMapSize);
            key_map_index %= kKeyMapSize;
        }

        key.insert(key.begin(), kKeyMap[key_map_index]);
    }

    if (containsN)
    {
        // Skip the first character.
        key.erase(key.begin());

        // Insert the symbol N after the first group.
        key.insert(kGroupLength, 1, 'N');
    }

    for (size_t i = kGroupLength; i < key.length(); i += kGroupLength + 1)
    {
        // Insert group separators.
        key.insert(i, 1, '-');
    }

    return key;
}

//--------------------------------------------------------------------------------------------------
bool msProductName(const QString& id, QString* product_name, REGSAM access)
{
    QString key_path = QString("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\%1").arg(id);

    RegKey key;
    LONG status = key.open(HKEY_LOCAL_MACHINE, key_path, access | KEY_READ);
    if (status != ERROR_SUCCESS)
        return false;

    status = key.readValue("DisplayName", product_name);
    if (status != ERROR_SUCCESS)
        return false;

    return true;
}

//--------------------------------------------------------------------------------------------------
void addMsProduct(proto::system_info::Licenses* message, const QString& product_name,
    const RegKey& key)
{
    DWORD product_id_size = 0;

    LONG status = key.readValue("DigitalProductId", nullptr, &product_id_size, nullptr);
    if (status != ERROR_SUCCESS)
    {
        status = key.readValue("DPID", nullptr, &product_id_size, nullptr);
        if (status != ERROR_SUCCESS)
            return;
    }

    std::unique_ptr<quint8[]> product_id = std::make_unique<quint8[]>(product_id_size);

    status = key.readValue("DigitalProductId", product_id.get(), &product_id_size, nullptr);
    if (status != ERROR_SUCCESS)
    {
        status = key.readValue("DPID", product_id.get(), &product_id_size, nullptr);
        if (status != ERROR_SUCCESS)
            return;
    }

    proto::system_info::Licenses::License* item = message->add_license();

    item->set_product_name(product_name.toStdString());

    proto::system_info::Licenses::License::Field* product_key = item->add_field();

    product_key->set_type(proto::system_info::Licenses::License::Field::TYPE_PRODUCT_KEY);
    product_key->set_value(digitalProductIdToString(product_id.get(), product_id_size));

    QString value;

    status = key.readValue("ProductId", &value);
    if (status == ERROR_SUCCESS)
    {
        proto::system_info::Licenses::License::Field* id = item->add_field();

        id->set_type(proto::system_info::Licenses::License::Field::TYPE_PRODUCT_ID);
        id->set_value(value.toStdString());
    }

    status = key.readValue("RegisteredOrganization", &value);
    if (status == ERROR_SUCCESS)
    {
        proto::system_info::Licenses::License::Field* organization = item->add_field();

        organization->set_type(proto::system_info::Licenses::License::Field::TYPE_ORGANIZATION);
        organization->set_value(value.toStdString());
    }

    status = key.readValue("RegisteredOwner", &value);
    if (status == ERROR_SUCCESS)
    {
        proto::system_info::Licenses::License::Field* owner = item->add_field();

        owner->set_type(proto::system_info::Licenses::License::Field::TYPE_OWNER);
        owner->set_value(value.toStdString());
    }
}

//--------------------------------------------------------------------------------------------------
void addMsProducts(proto::system_info::Licenses* message, REGSAM access)
{
    RegKey key;

    // Read MS Windows Key.
    LONG status = key.open(HKEY_LOCAL_MACHINE,
                           "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
                           access | KEY_READ);
    if (status == ERROR_SUCCESS)
    {
        OSInfo* os_info = OSInfo::instance();
        QString product_name;

        if (os_info->version() >= VERSION_WIN11)
        {
            // Key ProductName in the Windows 11 registry says it's Windows 10.
            // We can't rely on this value.
            switch (os_info->versionType())
            {
                case SUITE_HOME:
                    product_name = "Windows 11 Home";
                    break;

                case SUITE_PROFESSIONAL:
                    product_name = "Windows 11 Pro";
                    break;

                case SUITE_SERVER:
                    product_name = "Windows 11 Server";
                    break;

                case SUITE_ENTERPRISE:
                    product_name = "Windows 11 Enterprise";
                    break;

                case SUITE_EDUCATION:
                    product_name = "Windows 11 Education";
                    break;

                case SUITE_EDUCATION_PRO:
                    product_name = "Windows 11 Education Pro";
                    break;

                default:
                    product_name = "Windows 11";
                    break;
            }
        }
        else
        {
            key.readValue("ProductName", &product_name);
        }

        if (!product_name.isEmpty())
            addMsProduct(message, product_name, key);
    }

    static const char* kMsProducts[] =
    {
        "SOFTWARE\\Microsoft\\Microsoft SQL Server",
        "SOFTWARE\\Microsoft\\MSDN",
        "SOFTWARE\\Microsoft\\Office"
    };

    // Enumerate product family.
    for (size_t i = 0; i < _countof(kMsProducts); ++i)
    {
        RegKeyIterator key_iterator(HKEY_LOCAL_MACHINE, kMsProducts[i], access);

        // Enumerate product type.
        while (key_iterator.valid())
        {
            QString key_path =
                QString("%1\\%2\\Registration").arg(kMsProducts[i], key_iterator.name());

            RegKeyIterator sub_key_iterator(HKEY_LOCAL_MACHINE, key_path, access);

            // Enumerate product version.
            while (sub_key_iterator.valid())
            {
                QString product_name;

                if (msProductName(sub_key_iterator.name(), &product_name, access))
                {
                    QString sub_key_path =
                        QString("%1\\%2").arg(key_path, sub_key_iterator.name());

                    status = key.open(HKEY_LOCAL_MACHINE, sub_key_path, access | KEY_READ);
                    if (status == ERROR_SUCCESS)
                        addMsProduct(message, product_name, key);
                }

                ++sub_key_iterator;
            }

            ++key_iterator;
        }
    }
}

//--------------------------------------------------------------------------------------------------
void addVisualStudio(proto::system_info::Licenses* message, REGSAM access)
{
    static const char kVisualStudioPath[] = "SOFTWARE\\Microsoft\\VisualStudio";
    static const int kProductKeyLength = 25;
    static const int kGroupLength = 5;

    RegKeyIterator key_iterator(HKEY_LOCAL_MACHINE, kVisualStudioPath, access);

    while (key_iterator.valid())
    {
        QString key_path =
            QString("%1\\%2\\Registration").arg(kVisualStudioPath, key_iterator.name());

        RegKeyIterator sub_key_iterator(HKEY_LOCAL_MACHINE, key_path, access);

        while (sub_key_iterator.valid())
        {
            QString sub_key_path = QString("%1\\%2").arg(key_path, sub_key_iterator.name());
            RegKey key;

            LONG status = key.open(HKEY_LOCAL_MACHINE, sub_key_path, access | KEY_READ);
            if (status == ERROR_SUCCESS)
            {
                QString value;

                status = key.readValue("PIDKEY", &value);
                if (status == ERROR_SUCCESS && value.length() == kProductKeyLength)
                {
                    for (int i = kGroupLength; i < value.length(); i += kGroupLength + 1)
                    {
                        // Insert group separators.
                        value.insert(i, '-');
                    }

                    proto::system_info::Licenses::License* item = message->add_license();

                    item->set_product_name("Microsoft Visual Studio");

                    proto::system_info::Licenses::License::Field* product_key = item->add_field();

                    product_key->set_type(proto::system_info::Licenses::License::Field::TYPE_PRODUCT_KEY);
                    product_key->set_value(value.toStdString());

                    status = key.readValue("UserName", &value);
                    if (status == ERROR_SUCCESS)
                    {
                        proto::system_info::Licenses::License::Field* owner = item->add_field();

                        owner->set_type(proto::system_info::Licenses::License::Field::TYPE_OWNER);
                        owner->set_value(value.toStdString());
                    }
                }
            }

            ++sub_key_iterator;
        }

        ++key_iterator;
    }
}

//--------------------------------------------------------------------------------------------------
void addVMWareProduct(proto::system_info::Licenses* message, const RegKey& key)
{
    QString product_id;

    LONG status = key.readValue("ProductID", &product_id);
    if (status != ERROR_SUCCESS)
        return;

    QString value;

    status = key.readValue("Serial", &value);
    if (status != ERROR_SUCCESS)
        return;

    proto::system_info::Licenses::License* item = message->add_license();

    item->set_product_name(product_id.toStdString());

    proto::system_info::Licenses::License::Field* serial_field = item->add_field();
    serial_field->set_type(proto::system_info::Licenses::License::Field::TYPE_PRODUCT_KEY);
    serial_field->set_value(value.toStdString());

    status = key.readValue("LicenseVersion", &value);
    if (status == ERROR_SUCCESS)
    {
        proto::system_info::Licenses::License::Field* field = item->add_field();

        field->set_type(proto::system_info::Licenses::License::Field::TYPE_LICENSE_VERSION);
        field->set_value(value.toStdString());
    }

    status = key.readValue("LicenseType", &value);
    if (status == ERROR_SUCCESS)
    {
        proto::system_info::Licenses::License::Field* field = item->add_field();

        field->set_type(proto::system_info::Licenses::License::Field::TYPE_LICENSE_TYPE);
        field->set_value(value.toStdString());
    }
}

//--------------------------------------------------------------------------------------------------
void addVMWareProducts(proto::system_info::Licenses* message, REGSAM access)
{
    static const char kKeyPath[] = "Software\\VMware, Inc.";

    RegKeyIterator key_iterator(HKEY_LOCAL_MACHINE, kKeyPath, access);

    // Enumerate products types (Workstation, Server, etc).
    while (key_iterator.valid())
    {
        QString sub_key_path = QString("%1\\%2").arg(kKeyPath, key_iterator.name());

        RegKeyIterator sub_key_iterator(HKEY_LOCAL_MACHINE, sub_key_path, access);

        while (sub_key_iterator.valid())
        {
            if (sub_key_iterator.name().startsWith("License.ws"))
            {
                QString license_key_path =
                    QString("%1\\%2").arg(sub_key_path, sub_key_iterator.name());

                RegKey key;

                LONG status = key.open(HKEY_LOCAL_MACHINE, license_key_path, access | KEY_READ);
                if (status == ERROR_SUCCESS)
                    addVMWareProduct(message, key);
            }

            ++sub_key_iterator;
        }

        ++key_iterator;
    }
}
#endif // defined(Q_OS_WINDOWS)

} // namespace

//--------------------------------------------------------------------------------------------------
void readLicensesInformation(proto::system_info::Licenses* licenses)
{
#if defined(Q_OS_WINDOWS)
    addOemProduct(licenses);

#if defined(Q_PROCESSOR_X86_32)
    BOOL is_wow64;

    // If the x86 application is running in a x64 system.
    if (IsWow64Process(GetCurrentProcess(), &is_wow64) && is_wow64)
    {
        // We need to read the 64-bit keys.
        addMsProducts(licenses, KEY_WOW64_64KEY);
        addVisualStudio(licenses, KEY_WOW64_64KEY);
        addVMWareProducts(licenses, KEY_WOW64_64KEY);
    }
#elif defined(Q_PROCESSOR_X86_64)
    // If the x64 application is running in a x64 system we always read 32-bit keys.
    addMsProducts(licenses, KEY_WOW64_32KEY);
    addVisualStudio(licenses, KEY_WOW64_32KEY);
    addVMWareProducts(licenses, KEY_WOW64_32KEY);
#else
#error Unknown architecture
#endif

    // Read native keys.
    addMsProducts(licenses, 0);
    addVisualStudio(licenses, 0);
    addVMWareProducts(licenses, 0);
#elif defined(Q_OS_LINUX)
    addOemProduct(licenses);
#else
    Q_UNUSED(licenses)
    NOTIMPLEMENTED();
#endif
}
