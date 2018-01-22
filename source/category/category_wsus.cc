//
// PROJECT:         Aspia
// FILE:            category/category_wsus.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/strings/unicode.h"
#include "base/datetime.h"
#include "base/scoped_comptr.h"
#include "base/scoped_bstr.h"
#include "category/category_wsus.h"
#include "category/category_wsus.pb.h"
#include "ui/resource.h"

#include <wuapi.h>

namespace aspia {

namespace {

void AddUpdates(proto::WSUS& message)
{
    ScopedComPtr<IUpdateSession> update_session;

    HRESULT hr = update_session.CreateInstance(CLSID_UpdateSession);
    if (FAILED(hr))
    {
        DLOG(LS_ERROR) << "update_session.CreateInstance() failed: "
                       << SystemErrorCodeToString(hr);
        return;
    }

    ScopedComPtr<IUpdateSearcher> update_searcher;

    hr = update_session->CreateUpdateSearcher(update_searcher.Receive());
    if (FAILED(hr))
    {
        DLOG(LS_ERROR) << "update_session->CreateUpdateSearcher() failed: "
                       << SystemErrorCodeToString(hr);
        return;
    }

    LONG count = 0;

    hr = update_searcher->GetTotalHistoryCount(&count);
    if (FAILED(hr))
    {
        DLOG(LS_ERROR) << "update_searcher->GetTotalHistoryCount() failed: "
                       << SystemErrorCodeToString(hr);
        return;
    }

    ScopedComPtr<IUpdateHistoryEntryCollection> history;

    hr = update_searcher->QueryHistory(0, count, history.Receive());
    if (FAILED(hr))
    {
        DLOG(LS_ERROR) << "update_searcher->QueryHistory() failed: "
                       << SystemErrorCodeToString(hr);
        return;
    }

    hr = history->get_Count(&count);
    if (FAILED(hr))
    {
        DLOG(LS_ERROR) << "history->get_Count() failed: " << SystemErrorCodeToString(hr);
        return;
    }

    for (LONG i = 0; i < count; ++i)
    {
        ScopedComPtr<IUpdateHistoryEntry> entry;

        hr = history->get_Item(i, entry.Receive());
        if (FAILED(hr))
        {
            DLOG(LS_ERROR) << "history->get_Item() failed: " << SystemErrorCodeToString(hr);
            continue;
        }

        ScopedBstr description;

        hr = entry->get_Title(description.Receive());
        if (FAILED(hr))
        {
            DLOG(LS_ERROR) << "entry->get_Title() failed: " << SystemErrorCodeToString(hr);
            continue;
        }

        proto::WSUS::Item* item = message.add_item();

        item->set_description(UTF8fromUNICODE(description));

        DATE date;

        hr = entry->get_Date(&date);
        if (FAILED(hr))
        {
            DLOG(LS_ERROR) << "entry->get_Date() failed: " << SystemErrorCodeToString(hr);
            continue;
        }

        item->set_install_date(VariantTimeToUnixTime(date));
    }
}

} // namespace

CategoryWSUS::CategoryWSUS()
    : CategoryInfo(Type::INFO_LIST)
{
    // Nothing
}

const char* CategoryWSUS::Name() const
{
    return "Windows Update";
}

Category::IconId CategoryWSUS::Icon() const
{
    return IDI_APPLICATIONS;
}

const char* CategoryWSUS::Guid() const
{
    return "{1F47BC2F-D3A2-4330-9CA3-907ED1ED1AF5}";
}

void CategoryWSUS::Parse(Table& table, const std::string& data)
{
    proto::WSUS message;

    if (!message.ParseFromString(data))
        return;

    table.AddColumns(ColumnList::Create()
                     .AddColumn("Description", 450)
                     .AddColumn("Install Date", 150));

    for (int index = 0; index < message.item_size(); ++index)
    {
        const proto::WSUS::Item& item = message.item(index);

        Row row = table.AddRow();
        row.AddValue(Value::String(item.description()));
        row.AddValue(Value::String(TimeToString(item.install_date())));
    }
}

std::string CategoryWSUS::Serialize()
{
    proto::WSUS message;
    AddUpdates(message);
    return message.SerializeAsString();
}

} // namespace aspia
