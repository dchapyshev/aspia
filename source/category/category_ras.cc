//
// PROJECT:         Aspia
// FILE:            category/category_ras.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/strings/string_printf.h"
#include "base/strings/unicode.h"
#include "base/logging.h"
#include "category/category_ras.h"
#include "category/category_ras.pb.h"
#include "ui/resource.h"

#include <ras.h>
#include <raserror.h>
#include <strsafe.h>
#include <memory>

namespace aspia {

namespace {

std::string AddressToString(const RASIPADDR& address)
{
    if (address.a == 0 && address.b == 0 && address.c == 0 && address.d == 0)
        return std::string();

    return StringPrintf("%u.%u.%u.%u", address.a, address.b, address.c, address.d);
}

const char* FramingProtocolToString(proto::RAS::FramingProtocol value)
{
    switch (value)
    {
        case proto::RAS::FRAMING_PROTOCOL_PPP:
            return "PPP";

        case proto::RAS::FRAMING_PROTOCOL_SLIP:
            return "SLIP";

        default:
            return "Unknown";
    }
}

} // namespace

CategoryRAS::CategoryRAS()
    : CategoryInfo(Type::INFO_PARAM_VALUE)
{
    // Nothing
}

const char* CategoryRAS::Name() const
{
    return "RAS Connections";
}

Category::IconId CategoryRAS::Icon() const
{
    return IDI_TELEPHONE_FAX;
}

const char* CategoryRAS::Guid() const
{
    return "{E0A43CFD-3A97-4577-B3FB-3B542C0729F7}";
}

void CategoryRAS::Parse(Table& table, const std::string& data)
{
    proto::RAS message;

    if (!message.ParseFromString(data))
        return;

    table.AddColumns(ColumnList::Create()
                     .AddColumn("Parameter", 300)
                     .AddColumn("Value", 260));

    for (int index = 0; index < message.item_size(); ++index)
    {
        const proto::RAS::Item& item = message.item(index);

        Group group = table.AddGroup(item.connection_name());

        group.AddParam("Device Name", Value::String(item.device_name()));
        group.AddParam("Device Type", Value::String(item.device_type()));

        if (item.country_code())
            group.AddParam("Country Code", Value::Number(item.country_code()));

        if (!item.area_code().empty())
            group.AddParam("Area Code", Value::String(item.area_code()));

        group.AddParam((item.device_type() == "vpn") ? "Server" : "Phone Number",
                       Value::String(item.local_phone_number()));

        if (!item.user_name().empty())
            group.AddParam("User Name", Value::String(item.user_name()));

        if (!item.domain().empty())
            group.AddParam("Domain", Value::String(item.domain()));

        if (!item.ip_address().empty())
            group.AddParam("IP Address", Value::String(item.ip_address()));

        if (!item.dns_address_1().empty())
            group.AddParam("First DNS Server", Value::String(item.dns_address_1()));

        if (!item.dns_address_2().empty())
            group.AddParam("Second DNS Server", Value::String(item.dns_address_2()));

        if (!item.wins_address_1().empty())
            group.AddParam("First WINS Server", Value::String(item.wins_address_1()));

        if (!item.wins_address_2().empty())
            group.AddParam("Second WINS Server", Value::String(item.wins_address_2()));

        group.AddParam("Framing Protocol", Value::String(
            FramingProtocolToString(item.framing_protocol())));

        if (!item.script().empty())
            group.AddParam("Script", Value::String(item.script()));

        Group options_group = group.AddGroup("Options");

        auto add_option = [&](const char* name, proto::RAS::Options value)
        {
            options_group.AddParam(name, Value::Bool((item.options() & value) ? true : false));
        };

        add_option("IP Header compression", proto::RAS::OPTION_IP_HEADER_COMPRESSION);
        add_option("PPP LCP Extensions", proto::RAS::OPTION_PPP_LCP_EXTENSIONS);
        add_option("SW Compression", proto::RAS::OPTION_SW_COMPRESSION);
        add_option("Require Encrypted Password", proto::RAS::OPTION_REQUIRE_ENCRYPTED_PASSWORD);
        add_option("Require MS Encrypted Password", proto::RAS::OPTION_REQUIRE_MS_ENCRYPTED_PASSWORD);
        add_option("Require Data Encryption", proto::RAS::OPTION_REQUIRE_DATA_ENCRYPTION);
        add_option("Require EAP", proto::RAS::OPTION_REQUIRE_EAP);
        add_option("Require PAP", proto::RAS::OPTION_REQUIRE_PAP);
        add_option("Require SPAP", proto::RAS::OPTION_REQUIRE_SPAP);
        add_option("Require CHAP", proto::RAS::OPTION_REQUIRE_CHAP);
        add_option("Require MSCHAP", proto::RAS::OPTION_REQUIRE_MS_CHAP);
        add_option("Require MSCHAP2", proto::RAS::OPTION_REQUIRE_MS_CHAP_2);
    }
}

std::string CategoryRAS::Serialize()
{
    DWORD required_size = 0;
    DWORD total_entries = 0;

    DWORD error_code = RasEnumEntriesW(nullptr, nullptr, nullptr, &required_size, &total_entries);
    if (error_code != ERROR_BUFFER_TOO_SMALL)
    {
        DLOG(LS_WARNING) << "RasEnumEntriesW() failed: " << SystemErrorCodeToString(error_code);
        return std::string();
    }

    if (!required_size || !total_entries)
        return std::string();

    std::unique_ptr<uint8_t[]> entry_name_buffer = std::make_unique<uint8_t[]>(required_size);
    RASENTRYNAMEW* entry_name = reinterpret_cast<RASENTRYNAMEW*>(entry_name_buffer.get());

    entry_name->dwSize = sizeof(RASENTRYNAMEW);

    error_code = RasEnumEntriesW(nullptr, nullptr, entry_name, &required_size, &total_entries);
    if (error_code != ERROR_SUCCESS)
    {
        DLOG(LS_WARNING) << "RasEnumEntriesW() failed: " << SystemErrorCodeToString(error_code);
        return std::string();
    }

    proto::RAS message;

    for (DWORD i = 0; i < total_entries; ++i)
    {
        RASENTRYW entry;

        required_size = sizeof(entry);

        memset(&entry, 0, required_size);
        entry.dwSize = required_size;

        error_code = RasGetEntryPropertiesW(entry_name[i].szPhonebookPath,
                                            entry_name[i].szEntryName,
                                            &entry,
                                            &required_size,
                                            nullptr,
                                            nullptr);
        if (error_code != ERROR_SUCCESS)
        {
            DLOG(LS_WARNING) << "RasGetEntryPropertiesW() failed: "
                             << SystemErrorCodeToString(error_code);
            continue;
        }

        RASDIALPARAMSW dial_params;

        memset(&dial_params, 0, sizeof(dial_params));
        dial_params.dwSize = sizeof(dial_params);

        HRESULT hr = StringCbCopyW(dial_params.szEntryName,
                                   sizeof(dial_params.szEntryName),
                                   entry_name[i].szEntryName);
        if (FAILED(hr))
        {
            DLOG(LS_WARNING) << "StringCbCopyW() failed: " << SystemErrorCodeToString(hr);
            continue;
        }

        BOOL password = FALSE;

        error_code = RasGetEntryDialParamsW(entry_name[i].szPhonebookPath, &dial_params, &password);
        if (error_code != ERROR_SUCCESS)
        {
            DLOG(LS_WARNING) << "RasGetEntryDialParamsW() failed: "
                             << SystemErrorCodeToString(error_code);
            continue;
        }

        proto::RAS::Item* item = message.add_item();

        item->set_connection_name(UTF8fromUNICODE(entry_name[i].szEntryName));
        item->set_device_name(UTF8fromUNICODE(entry.szDeviceName));
        item->set_device_type(UTF8fromUNICODE(entry.szDeviceType));
        item->set_country_code(entry.dwCountryCode);
        item->set_area_code(UTF8fromUNICODE(entry.szAreaCode));
        item->set_local_phone_number(UTF8fromUNICODE(entry.szLocalPhoneNumber));
        item->set_user_name(UTF8fromUNICODE(dial_params.szUserName));
        item->set_domain(UTF8fromUNICODE(dial_params.szDomain));
        item->set_ip_address(AddressToString(entry.ipaddr));
        item->set_dns_address_1(AddressToString(entry.ipaddrDns));
        item->set_dns_address_2(AddressToString(entry.ipaddrDnsAlt));
        item->set_wins_address_1(AddressToString(entry.ipaddrWins));
        item->set_wins_address_2(AddressToString(entry.ipaddrWinsAlt));

        switch (entry.dwFramingProtocol)
        {
            case RASFP_Ppp:
                item->set_framing_protocol(proto::RAS::FRAMING_PROTOCOL_PPP);
                break;

            case RASFP_Slip:
                item->set_framing_protocol(proto::RAS::FRAMING_PROTOCOL_SLIP);
                break;

            default:
                break;
        }

        item->set_script(UTF8fromUNICODE(entry.szScript));

        uint32_t options = 0;

        if (entry.dwfOptions & RASEO_IpHeaderCompression)
            options |= proto::RAS::OPTION_IP_HEADER_COMPRESSION;

        if (!(entry.dwfOptions & RASEO_DisableLcpExtensions))
            options |= proto::RAS::OPTION_PPP_LCP_EXTENSIONS;

        if (entry.dwfOptions & RASEO_SwCompression)
            options |= proto::RAS::OPTION_SW_COMPRESSION;

        if (entry.dwfOptions & RASEO_RequireEncryptedPw)
            options |= proto::RAS::OPTION_REQUIRE_ENCRYPTED_PASSWORD;

        if (entry.dwfOptions & RASEO_RequireMsEncryptedPw)
            options |= proto::RAS::OPTION_REQUIRE_MS_ENCRYPTED_PASSWORD;

        if (entry.dwfOptions & RASEO_RequireDataEncryption)
            options |= proto::RAS::OPTION_REQUIRE_DATA_ENCRYPTION;

        if (entry.dwfOptions & RASEO_RequireEAP)
            options |= proto::RAS::OPTION_REQUIRE_EAP;

        if (entry.dwfOptions & RASEO_RequirePAP)
            options |= proto::RAS::OPTION_REQUIRE_PAP;

        if (entry.dwfOptions & RASEO_RequireSPAP)
            options |= proto::RAS::OPTION_REQUIRE_SPAP;

        if (entry.dwfOptions & RASEO_RequireCHAP)
            options |= proto::RAS::OPTION_REQUIRE_CHAP;

        if (entry.dwfOptions & RASEO_RequireMsCHAP)
            options |= proto::RAS::OPTION_REQUIRE_MS_CHAP;

        if (entry.dwfOptions & RASEO_RequireMsCHAP2)
            options |= proto::RAS::OPTION_REQUIRE_MS_CHAP_2;

        item->set_options(options);
    }

    return message.SerializeAsString();
}

} // namespace aspia
