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

#include "base/license_reader.h"

#include "base/logging.h"
#include "base/strings/unicode.h"
#include "base/win/registry.h"
#include "base/win/windows_version.h"

#include <fmt/format.h>
#include <fmt/xchar.h>

namespace base {

namespace {

//--------------------------------------------------------------------------------------------------
std::string digitalProductIdToString(uint8_t* product_id, size_t product_id_size)
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
        static_cast<uint8_t>((product_id[kStartIndex + 14] & 0xF7) | ((containsN & 2) << 2));

    std::string key;

    for (int i = kDecodeLength - 1; i >= 0; --i)
    {
        int key_map_index = 0;

        for (int j = kDecodeStringLength - 1; j >= 0; --j)
        {
            key_map_index = (key_map_index << 8) | product_id[kStartIndex + j];
            product_id[kStartIndex + j] = static_cast<uint8_t>(key_map_index / kKeyMapSize);
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
bool msProductName(const wchar_t* id, std::wstring* product_name, REGSAM access)
{
    std::wstring key_path =
        fmt::format(L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\{}", id);

    RegistryKey key;
    LONG status = key.open(HKEY_LOCAL_MACHINE, key_path.c_str(), access | KEY_READ);
    if (status != ERROR_SUCCESS)
        return false;

    status = key.readValue(L"DisplayName", product_name);
    if (status != ERROR_SUCCESS)
        return false;

    return true;
}

//--------------------------------------------------------------------------------------------------
void addMsProduct(proto::system_info::Licenses* message,
                  const std::wstring& product_name,
                  const RegistryKey& key)
{
    DWORD product_id_size = 0;

    LONG status = key.readValue(L"DigitalProductId", nullptr, &product_id_size, nullptr);
    if (status != ERROR_SUCCESS)
    {
        status = key.readValue(L"DPID", nullptr, &product_id_size, nullptr);
        if (status != ERROR_SUCCESS)
            return;
    }

    std::unique_ptr<uint8_t[]> product_id = std::make_unique<uint8_t[]>(product_id_size);

    status = key.readValue(L"DigitalProductId", product_id.get(), &product_id_size, nullptr);
    if (status != ERROR_SUCCESS)
    {
        status = key.readValue(L"DPID", product_id.get(), &product_id_size, nullptr);
        if (status != ERROR_SUCCESS)
            return;
    }

    proto::system_info::Licenses::License* item = message->add_license();

    item->set_product_name(utf8FromWide(product_name));

    proto::system_info::Licenses::License::Field* product_key = item->add_field();

    product_key->set_type(proto::system_info::Licenses::License::Field::TYPE_PRODUCT_KEY);
    product_key->set_value(digitalProductIdToString(product_id.get(), product_id_size));

    std::wstring value;

    status = key.readValue(L"ProductId", &value);
    if (status == ERROR_SUCCESS)
    {
        proto::system_info::Licenses::License::Field* id = item->add_field();

        id->set_type(proto::system_info::Licenses::License::Field::TYPE_PRODUCT_ID);
        id->set_value(utf8FromWide(value));
    }

    status = key.readValue(L"RegisteredOrganization", &value);
    if (status == ERROR_SUCCESS)
    {
        proto::system_info::Licenses::License::Field* organization = item->add_field();

        organization->set_type(proto::system_info::Licenses::License::Field::TYPE_ORGANIZATION);
        organization->set_value(utf8FromWide(value));
    }

    status = key.readValue(L"RegisteredOwner", &value);
    if (status == ERROR_SUCCESS)
    {
        proto::system_info::Licenses::License::Field* owner = item->add_field();

        owner->set_type(proto::system_info::Licenses::License::Field::TYPE_OWNER);
        owner->set_value(utf8FromWide(value));
    }
}

//--------------------------------------------------------------------------------------------------
void addMsProducts(proto::system_info::Licenses* message, REGSAM access)
{
    RegistryKey key;

    // Read MS Windows Key.
    LONG status = key.open(HKEY_LOCAL_MACHINE,
                           L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
                           access | KEY_READ);
    if (status == ERROR_SUCCESS)
    {
        base::OSInfo* os_info = base::OSInfo::instance();
        std::wstring product_name;

        if (os_info->version() >= base::VERSION_WIN11)
        {
            // Key ProductName in the Windows 11 registry says it's Windows 10.
            // We can't rely on this value.
            switch (os_info->versionType())
            {
                case base::SUITE_HOME:
                    product_name = L"Windows 11 Home";
                    break;

                case base::SUITE_PROFESSIONAL:
                    product_name = L"Windows 11 Pro";
                    break;

                case base::SUITE_SERVER:
                    product_name = L"Windows 11 Server";
                    break;

                case base::SUITE_ENTERPRISE:
                    product_name = L"Windows 11 Enterprise";
                    break;

                case base::SUITE_EDUCATION:
                    product_name = L"Windows 11 Education";
                    break;

                case base::SUITE_EDUCATION_PRO:
                    product_name = L"Windows 11 Education Pro";
                    break;

                default:
                    product_name = L"Windows 11";
                    break;
            }
        }
        else
        {
            key.readValue(L"ProductName", &product_name);
        }

        if (!product_name.empty())
            addMsProduct(message, product_name, key);
    }

    static const wchar_t* kMsProducts[] =
    {
        L"SOFTWARE\\Microsoft\\Microsoft SQL Server",
        L"SOFTWARE\\Microsoft\\MSDN",
        L"SOFTWARE\\Microsoft\\Office"
    };

    // Enumerate product family.
    for (size_t i = 0; i < _countof(kMsProducts); ++i)
    {
        RegistryKeyIterator key_iterator(HKEY_LOCAL_MACHINE, kMsProducts[i], access);

        // Enumerate product type.
        while (key_iterator.valid())
        {
            std::wstring key_path =
                fmt::format(L"{}\\{}\\Registration", kMsProducts[i], key_iterator.name());

            RegistryKeyIterator sub_key_iterator(HKEY_LOCAL_MACHINE, key_path.c_str(), access);

            // Enumerate product version.
            while (sub_key_iterator.valid())
            {
                std::wstring product_name;

                if (msProductName(sub_key_iterator.name(), &product_name, access))
                {
                    std::wstring sub_key_path =
                        fmt::format(L"{}\\{}", key_path, sub_key_iterator.name());

                    status = key.open(HKEY_LOCAL_MACHINE, sub_key_path.c_str(), access | KEY_READ);
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
    static const wchar_t kVisualStudioPath[] = L"SOFTWARE\\Microsoft\\VisualStudio";
    static const int kProductKeyLength = 25;
    static const int kGroupLength = 5;

    RegistryKeyIterator key_iterator(HKEY_LOCAL_MACHINE, kVisualStudioPath, access);

    while (key_iterator.valid())
    {
        std::wstring key_path =
            fmt::format(L"{}\\{}\\Registration", kVisualStudioPath, key_iterator.name());

        RegistryKeyIterator sub_key_iterator(HKEY_LOCAL_MACHINE, key_path.c_str(), access);

        while (sub_key_iterator.valid())
        {
            std::wstring sub_key_path = fmt::format(L"{}\\{}", key_path, sub_key_iterator.name());
            RegistryKey key;

            LONG status = key.open(HKEY_LOCAL_MACHINE, sub_key_path.c_str(), access | KEY_READ);
            if (status == ERROR_SUCCESS)
            {
                std::wstring value;

                status = key.readValue(L"PIDKEY", &value);
                if (status == ERROR_SUCCESS && value.length() == kProductKeyLength)
                {
                    for (size_t i = kGroupLength; i < value.length(); i += kGroupLength + 1)
                    {
                        // Insert group separators.
                        value.insert(i, 1, '-');
                    }

                    proto::system_info::Licenses::License* item = message->add_license();

                    item->set_product_name("Microsoft Visual Studio");

                    proto::system_info::Licenses::License::Field* product_key = item->add_field();

                    product_key->set_type(proto::system_info::Licenses::License::Field::TYPE_PRODUCT_KEY);
                    product_key->set_value(utf8FromWide(value));

                    status = key.readValue(L"UserName", &value);
                    if (status == ERROR_SUCCESS)
                    {
                        proto::system_info::Licenses::License::Field* owner = item->add_field();

                        owner->set_type(proto::system_info::Licenses::License::Field::TYPE_OWNER);
                        owner->set_value(utf8FromWide(value));
                    }
                }
            }

            ++sub_key_iterator;
        }

        ++key_iterator;
    }
}

//--------------------------------------------------------------------------------------------------
void addVMWareProduct(proto::system_info::Licenses* message, const RegistryKey& key)
{
    std::wstring product_id;

    LONG status = key.readValue(L"ProductID", &product_id);
    if (status != ERROR_SUCCESS)
        return;

    std::wstring value;

    status = key.readValue(L"Serial", &value);
    if (status != ERROR_SUCCESS)
        return;

    proto::system_info::Licenses::License* item = message->add_license();

    item->set_product_name(utf8FromWide(product_id));

    proto::system_info::Licenses::License::Field* serial_field = item->add_field();
    serial_field->set_type(proto::system_info::Licenses::License::Field::TYPE_PRODUCT_KEY);
    serial_field->set_value(utf8FromWide(value));

    status = key.readValue(L"LicenseVersion", &value);
    if (status == ERROR_SUCCESS)
    {
        proto::system_info::Licenses::License::Field* field = item->add_field();

        field->set_type(proto::system_info::Licenses::License::Field::TYPE_LICENSE_VERSION);
        field->set_value(utf8FromWide(value));
    }

    status = key.readValue(L"LicenseType", &value);
    if (status == ERROR_SUCCESS)
    {
        proto::system_info::Licenses::License::Field* field = item->add_field();

        field->set_type(proto::system_info::Licenses::License::Field::TYPE_LICENSE_TYPE);
        field->set_value(utf8FromWide(value));
    }
}

//--------------------------------------------------------------------------------------------------
void addVMWareProducts(proto::system_info::Licenses* message, REGSAM access)
{
    static const wchar_t kKeyPath[] = L"Software\\VMware, Inc.";

    RegistryKeyIterator key_iterator(HKEY_LOCAL_MACHINE, kKeyPath, access);

    // Enumerate products types (Workstation, Server, etc).
    while (key_iterator.valid())
    {
        std::wstring sub_key_path = fmt::format(L"{}\\{}", kKeyPath, key_iterator.name());

        RegistryKeyIterator sub_key_iterator(HKEY_LOCAL_MACHINE, sub_key_path.c_str(), access);

        while (sub_key_iterator.valid())
        {
            if (wcsncmp(sub_key_iterator.name(), L"License.ws", 10) == 0)
            {
                std::wstring license_key_path =
                    fmt::format(L"{}\\{}", sub_key_path, sub_key_iterator.name());

                RegistryKey key;

                LONG status = key.open(HKEY_LOCAL_MACHINE,
                                       license_key_path.c_str(),
                                       access | KEY_READ);
                if (status == ERROR_SUCCESS)
                    addVMWareProduct(message, key);
            }

            ++sub_key_iterator;
        }

        ++key_iterator;
    }
}

} // namespace

//--------------------------------------------------------------------------------------------------
void readLicensesInformation(proto::system_info::Licenses* licenses)
{
#if (ARCH_CPU_X86 == 1)
    BOOL is_wow64;

    // If the x86 application is running in a x64 system.
    if (IsWow64Process(GetCurrentProcess(), &is_wow64) && is_wow64)
    {
        // We need to read the 64-bit keys.
        addMsProducts(licenses, KEY_WOW64_64KEY);
        addVisualStudio(licenses, KEY_WOW64_64KEY);
        addVMWareProducts(licenses, KEY_WOW64_64KEY);
    }
#elif (ARCH_CPU_X86_64 == 1)
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
}

} // namespace base
