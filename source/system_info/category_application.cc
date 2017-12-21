//
// PROJECT:         Aspia
// FILE:            system_info/category_application.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/strings/string_printf.h"
#include "base/strings/unicode.h"
#include "base/registry.h"
#include "base/logging.h"
#include "system_info/category_application.h"
#include "system_info/category_application.pb.h"
#include "ui/resource.h"

namespace aspia {

namespace {

constexpr WCHAR kUninstallKeyPath[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall";

constexpr WCHAR kDisplayName[] = L"DisplayName";
constexpr WCHAR kDisplayVersion[] = L"DisplayVersion";
constexpr WCHAR kPublisher[] = L"Publisher";
constexpr WCHAR kInstallDate[] = L"InstallDate";
constexpr WCHAR kInstallLocation[] = L"InstallLocation";
constexpr WCHAR kSystemComponent[] = L"SystemComponent";
constexpr WCHAR kParentKeyName[] = L"ParentKeyName";

bool AddApplication(proto::Application* message, const WCHAR* key_name)
{
    std::wstring key_path = StringPrintf(L"%s\\%s", kUninstallKeyPath, key_name);

    RegistryKey key;

    LONG status = key.Open(HKEY_LOCAL_MACHINE, key_path.c_str(), KEY_READ);
    if (status != ERROR_SUCCESS)
    {
        LOG(WARNING) << "Unable to open registry key: " << SystemErrorCodeToString(status);
        return false;
    }

    DWORD system_component = 0;

    status = key.ReadValueDW(kSystemComponent, &system_component);
    if (status == ERROR_SUCCESS && system_component == 1)
        return false;

    std::wstring value;

    status = key.ReadValue(kParentKeyName, &value);
    if (status == ERROR_SUCCESS)
        return false;

    status = key.ReadValue(kDisplayName, &value);
    if (status != ERROR_SUCCESS)
        return false;

    proto::Application::Item* item = message->add_item();

    item->set_name(UTF8fromUNICODE(value));

    status = key.ReadValue(kDisplayVersion, &value);
    if (status == ERROR_SUCCESS)
        item->set_version(UTF8fromUNICODE(value));

    status = key.ReadValue(kPublisher, &value);
    if (status == ERROR_SUCCESS)
        item->set_publisher(UTF8fromUNICODE(value));

    status = key.ReadValue(kInstallDate, &value);
    if (status == ERROR_SUCCESS)
        item->set_install_date(UTF8fromUNICODE(value));

    status = key.ReadValue(kInstallLocation, &value);
    if (status == ERROR_SUCCESS)
        item->set_install_location(UTF8fromUNICODE(value));

    return true;
}

} // namespace

CategoryApplication::CategoryApplication()
    : CategoryInfo(Type::INFO_LIST)
{
    // Nothing
}

const char* CategoryApplication::Name() const
{
    return "Applications";
}

Category::IconId CategoryApplication::Icon() const
{
    return IDI_APPLICATIONS;
}

const char* CategoryApplication::Guid() const
{
    return "{606C70BE-0C6C-4CB6-90E6-D374760FC5EE}";
}

void CategoryApplication::Parse(Table& table, const std::string& data)
{
    proto::Application message;

    if (!message.ParseFromString(data))
        return;

    table.AddColumns(ColumnList::Create()
                     .AddColumn("Name", 200)
                     .AddColumn("Version", 100)
                     .AddColumn("Publisher", 100)
                     .AddColumn("Install Date", 80)
                     .AddColumn("Install Location", 150));

    std::vector<std::pair<std::string, int>> list;

    for (int index = 0; index < message.item_size(); ++index)
    {
        std::string* name = message.mutable_item(index)->release_name();
        list.emplace_back(std::move(*name), index);
    }

    std::sort(list.begin(), list.end());

    for (const auto& list_item : list)
    {
        const proto::Application::Item& item = message.item(list_item.second);

        Row row = table.AddRow();
        row.AddValue(Value::String(list_item.first));
        row.AddValue(Value::String(item.version()));
        row.AddValue(Value::String(item.publisher()));
        row.AddValue(Value::String(item.install_date()));
        row.AddValue(Value::String(item.install_location()));
    }
}

std::string CategoryApplication::Serialize()
{
    proto::Application message;

    RegistryKeyIterator machine_key_iterator(HKEY_LOCAL_MACHINE, kUninstallKeyPath);

    while (machine_key_iterator.Valid())
    {
        AddApplication(&message, machine_key_iterator.Name());
        ++machine_key_iterator;
    }

    RegistryKeyIterator user_key_iterator(HKEY_CURRENT_USER, kUninstallKeyPath);

    while (user_key_iterator.Valid())
    {
        AddApplication(&message, user_key_iterator.Name());
        ++user_key_iterator;
    }

#if (ARCH_CPU_X86 == 1)

    RegistryKeyIterator machine64_key_iterator(HKEY_LOCAL_MACHINE,
                                               kUninstallKeyPath,
                                               KEY_WOW64_64KEY);

    while (machine64_key_iterator.Valid())
    {
        AddApplication(&message, machine64_key_iterator.Name());
        ++machine64_key_iterator;
    }

    RegistryKeyIterator user64_key_iterator(HKEY_CURRENT_USER,
                                            kUninstallKeyPath,
                                            KEY_WOW64_64KEY);

    while (user64_key_iterator.Valid())
    {
        AddApplication(&message, user64_key_iterator.Name());
        ++user64_key_iterator;
    }

#elif (ARCH_CPU_X86_64 == 1)

    RegistryKeyIterator machine32_key_iterator(HKEY_LOCAL_MACHINE,
                                               kUninstallKeyPath,
                                               KEY_WOW64_32KEY);

    while (machine32_key_iterator.Valid())
    {
        AddApplication(&message, machine32_key_iterator.Name());
        ++machine32_key_iterator;
    }

    RegistryKeyIterator user32_key_iterator(HKEY_CURRENT_USER,
                                            kUninstallKeyPath,
                                            KEY_WOW64_32KEY);

    while (user32_key_iterator.Valid())
    {
        AddApplication(&message, user32_key_iterator.Name());
        ++user32_key_iterator;
    }

#else
#error Unknown Architecture
#endif

    return message.SerializeAsString();
}

} // namespace aspia
