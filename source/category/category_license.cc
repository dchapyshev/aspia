//
// PROJECT:         Aspia
// FILE:            category/category_license.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/strings/string_printf.h"
#include "base/strings/unicode.h"
#include "base/registry.h"
#include "base/logging.h"
#include "category/category_license.h"
#include "category/category_license.pb.h"
#include "ui/resource.h"

namespace aspia {

namespace {

std::string DigitalProductIdToString(uint8_t* product_id, size_t product_id_size)
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
            int value = (key_map_index << 8) | product_id[kStartIndex + j];
            product_id[kStartIndex + j] = static_cast<uint8_t>(value / kKeyMapSize);
            key_map_index = value % kKeyMapSize;
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

void AddMsProduct(proto::License& message, const std::wstring& product_name, const RegistryKey& key)
{
    DWORD product_id_size = 0;

    LONG status = key.ReadValue(L"DigitalProductId", nullptr, &product_id_size, nullptr);
    if (status != ERROR_SUCCESS)
        return;

    std::unique_ptr<uint8_t[]> product_id = std::make_unique<uint8_t[]>(product_id_size);

    status = key.ReadValue(L"DigitalProductId", product_id.get(), &product_id_size, nullptr);
    if (status != ERROR_SUCCESS)
        return;

    proto::License::Item* item = message.add_item();

    item->set_product_name(UTF8fromUNICODE(product_name));

    proto::License::Field* product_key = item->add_field();

    product_key->set_type(proto::License::Field::TYPE_PRODUCT_KEY);
    product_key->set_value(DigitalProductIdToString(product_id.get(), product_id_size));

    std::wstring value;

    status = key.ReadValue(L"ProductId", &value);
    if (status == ERROR_SUCCESS)
    {
        proto::License::Field* id = item->add_field();

        id->set_type(proto::License::Field::TYPE_PRODUCT_ID);
        id->set_value(UTF8fromUNICODE(value));
    }

    status = key.ReadValue(L"RegisteredOrganization", &value);
    if (status == ERROR_SUCCESS)
    {
        proto::License::Field* organization = item->add_field();

        organization->set_type(proto::License::Field::TYPE_ORGANIZATION);
        organization->set_value(UTF8fromUNICODE(value));
    }

    status = key.ReadValue(L"RegisteredOwner", &value);
    if (status == ERROR_SUCCESS)
    {
        proto::License::Field* owner = item->add_field();

        owner->set_type(proto::License::Field::TYPE_OWNER);
        owner->set_value(UTF8fromUNICODE(value));
    }
}

bool GetMsProductName(const WCHAR* id, std::wstring* product_name, REGSAM access)
{
    std::wstring key_path =
        StringPrintf(L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\%s", id);

    RegistryKey key;
    LONG status = key.Open(HKEY_LOCAL_MACHINE, key_path.c_str(), access | KEY_READ);
    if (status != ERROR_SUCCESS)
        return false;

    status = key.ReadValue(L"DisplayName", product_name);
    if (status != ERROR_SUCCESS)
        return false;

    return true;
}

void AddMsProducts(proto::License& message, REGSAM access)
{
    RegistryKey key;

    // Read MS Windows Key.
    LONG status = key.Open(HKEY_LOCAL_MACHINE,
                           L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
                           access | KEY_READ);
    if (status == ERROR_SUCCESS)
    {
        std::wstring product_name;

        status = key.ReadValue(L"ProductName", &product_name);
        if (status == ERROR_SUCCESS)
            AddMsProduct(message, product_name, key);
    }

    static const WCHAR* kMsProducts[] =
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
        while (key_iterator.Valid())
        {
            std::wstring key_path =
                StringPrintf(L"%s\\%s\\Registration", kMsProducts[i], key_iterator.Name());

            RegistryKeyIterator sub_key_iterator(HKEY_LOCAL_MACHINE, key_path.c_str(), access);

            // Enumerate product version.
            while (sub_key_iterator.Valid())
            {
                std::wstring product_name;

                if (GetMsProductName(sub_key_iterator.Name(), &product_name, access))
                {
                    std::wstring sub_key_path =
                        StringPrintf(L"%s\\%s", key_path.c_str(), sub_key_iterator.Name());

                    status = key.Open(HKEY_LOCAL_MACHINE, sub_key_path.c_str(), access | KEY_READ);
                    if (status == ERROR_SUCCESS)
                        AddMsProduct(message, product_name, key);
                }

                ++sub_key_iterator;
            }

            ++key_iterator;
        }
    }
}

const char* FieldTypeToString(proto::License::Field::Type value)
{
    switch (value)
    {
        case proto::License::Field::TYPE_OWNER:
            return "Owner";

        case proto::License::Field::TYPE_ORGANIZATION:
            return "Organization";

        case proto::License::Field::TYPE_PRODUCT_KEY:
            return "Product Key";

        case proto::License::Field::TYPE_PRODUCT_ID:
            return "Product Id";

        default:
            return "Unknown";
    }
}

} // namespace

CategoryLicense::CategoryLicense()
    : CategoryInfo(Type::INFO_PARAM_VALUE)
{
    // Nothing
}

const char* CategoryLicense::Name() const
{
    return "Licenses";
}

Category::IconId CategoryLicense::Icon() const
{
    return IDI_LICENSE_KEY;
}

const char* CategoryLicense::Guid() const
{
    return "{6BD88575-9D23-44BC-8A49-64D94CC3EE48}";
}

void CategoryLicense::Parse(Table& table, const std::string& data)
{
    proto::License message;

    if (!message.ParseFromString(data))
        return;

    table.AddColumns(ColumnList::Create()
                     .AddColumn("Parameter", 300)
                     .AddColumn("Value", 260));

    for (int index = 0; index < message.item_size(); ++index)
    {
        const proto::License::Item& item = message.item(index);

        Group group = table.AddGroup(item.product_name());

        for (int i = 0; i < item.field_size(); ++i)
        {
            const proto::License::Field& field = item.field(i);

            group.AddParam(FieldTypeToString(field.type()), Value::String(field.value()));
        }
    }
}

std::string CategoryLicense::Serialize()
{
    proto::License message;

#if (ARCH_CPU_X86 == 1)

    BOOL is_wow64;

    // If the x86 application is running in a x64 system.
    if (IsWow64Process(GetCurrentProcess(), &is_wow64) && is_wow64)
    {
        // We need to read the 64-bit keys.
        AddMsProducts(message, KEY_WOW64_64KEY);
    }

#elif (ARCH_CPU_X86_64 == 1)
    // If the x64 application is running in a x64 system we always read 32-bit keys.
    AddMsProducts(message, KEY_WOW64_32KEY);
#else
#error Unknown architecture
#endif

    // Read native keys.
    AddMsProducts(message, 0);

    return message.SerializeAsString();
}

} // namespace aspia
