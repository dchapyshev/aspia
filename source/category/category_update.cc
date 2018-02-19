//
// PROJECT:         Aspia
// FILE:            category/category_update.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/strings/unicode.h"
#include "base/logging.h"
#include "category/category_update.h"
#include "category/category_update.pb.h"
#include "ui/resource.h"

#include <msi.h>

namespace aspia {

CategoryUpdate::CategoryUpdate()
    : CategoryInfo(Type::INFO_LIST)
{
    // Nothing
}

const char* CategoryUpdate::Name() const
{
    return "Updates";
}

Category::IconId CategoryUpdate::Icon() const
{
    return IDI_APPLICATIONS;
}

const char* CategoryUpdate::Guid() const
{
    return "{3E160E27-BE2E-45DB-8292-C3786C9533AB}";
}

void CategoryUpdate::Parse(Table& table, const std::string& data)
{
    proto::Update message;

    if (!message.ParseFromString(data))
        return;

    table.AddColumns(ColumnList::Create()
                     .AddColumn("Description", 450)
                     .AddColumn("Install Date", 150));

    for (int index = 0; index < message.item_size(); ++index)
    {
        const proto::Update::Item& item = message.item(index);

        Row row = table.AddRow();
        row.AddValue(Value::String(item.description()));
        row.AddValue(Value::String(item.install_date()));
    }
}

std::string CategoryUpdate::Serialize()
{
    proto::Update message;

    DWORD product_index = 0;

    for (;;)
    {
        wchar_t product_guid[38 + 1];

        UINT ret = MsiEnumProductsW(product_index, product_guid);
        if (ret == ERROR_NO_MORE_ITEMS)
            break;

        if (ret != ERROR_SUCCESS)
        {
            DLOG(LS_WARNING) << "MsiEnumProductsW failed: " << SystemErrorCodeToString(ret);
            break;
        }

        DWORD patch_index = 0;

        for (;;)
        {
            wchar_t patch_guid[38 + 1];
            wchar_t patch_transforms[256];
            DWORD patch_transforms_length = ARRAYSIZE(patch_transforms);

            ret = MsiEnumPatchesW(product_guid, patch_index, patch_guid,
                                  patch_transforms, &patch_transforms_length);
            if (ret == ERROR_NO_MORE_ITEMS)
                break;

            if (ret != ERROR_SUCCESS)
            {
                DLOG(LS_WARNING) << "MsiEnumPatchesW failed: " << SystemErrorCodeToString(ret);
                break;
            }

            wchar_t value[256];
            DWORD value_length = ARRAYSIZE(value);

            ret = MsiGetPatchInfoExW(patch_guid, product_guid, nullptr, MSIINSTALLCONTEXT_MACHINE,
                                     INSTALLPROPERTY_DISPLAYNAME,
                                     value, &value_length);
            if (ret == ERROR_SUCCESS)
            {
                proto::Update::Item* item = message.add_item();

                item->set_description(UTF8fromUNICODE(value));

                value_length = ARRAYSIZE(value);

                ret = MsiGetPatchInfoExW(patch_guid, product_guid, nullptr, MSIINSTALLCONTEXT_MACHINE,
                                         INSTALLPROPERTY_INSTALLDATE,
                                         value, &value_length);
                if (ret == ERROR_SUCCESS)
                {
                    item->set_install_date(UTF8fromUNICODE(value));
                }
            }

            ++patch_index;
        }

        ++product_index;
    }

    return message.SerializeAsString();
}

} // namespace aspia
