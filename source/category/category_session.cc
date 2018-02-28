//
// PROJECT:         Aspia
// FILE:            category/category_session.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/strings/unicode.h"
#include "base/scoped_wts_memory.h"
#include "base/logging.h"
#include "category/category_session.h"
#include "category/category_session.pb.h"
#include "ui/resource.h"

namespace aspia {

namespace {

const char* ConnectStateToString(proto::Session::ConnectState value)
{
    switch (value)
    {
        case proto::Session::CONNECT_STATE_ACTIVE:
            return "Active";

        case proto::Session::CONNECT_STATE_CONNECTED:
            return "Connected";

        case proto::Session::CONNECT_STATE_CONNECT_QUERY:
            return "Connect Query";

        case proto::Session::CONNECT_STATE_SHADOW:
            return "Shadow";

        case proto::Session::CONNECT_STATE_DISCONNECTED:
            return "Disconnected";

        case proto::Session::CONNECT_STATE_IDLE:
            return "Idle";

        case proto::Session::CONNECT_STATE_LISTEN:
            return "Listen";

        case proto::Session::CONNECT_STATE_RESET:
            return "Reset";

        case proto::Session::CONNECT_STATE_DOWN:
            return "Down";

        case proto::Session::CONNECT_STATE_INIT:
            return "Init";

        default:
            return "Unknown";
    }
}

} // namespace

CategorySession::CategorySession()
    : CategoryInfo(Type::INFO_LIST)
{
    // Nothing
}

const char* CategorySession::Name() const
{
    return "Active Sessions";
}

Category::IconId CategorySession::Icon() const
{
    return IDI_USERS;
}

const char* CategorySession::Guid() const
{
    return "{8702E4A1-C9A2-4BA3-BBDE-CFCB6937D2C8}";
}

void CategorySession::Parse(Table& table, const std::string& data)
{
    proto::Session message;

    if (!message.ParseFromString(data))
        return;

    table.AddColumns(ColumnList::Create()
                     .AddColumn("User Name", 150)
                     .AddColumn("Domain", 100)
                     .AddColumn("ID", 50)
                     .AddColumn("State", 80)
                     .AddColumn("Client Name", 100)
                     .AddColumn("Logon Type", 100));

    for (int index = 0; index < message.item_size(); ++index)
    {
        const proto::Session::Item& item = message.item(index);

        Row row = table.AddRow();
        row.AddValue(Value::String(item.user_name()));
        row.AddValue(Value::String(item.domain_name()));
        row.AddValue(Value::Number(item.session_id()));
        row.AddValue(Value::String(ConnectStateToString(item.connect_state())));
        row.AddValue(Value::String(item.client_name()));
        row.AddValue(Value::String(item.winstation_name()));
    }
}

std::string CategorySession::Serialize()
{
    ScopedWtsMemory<PWTS_SESSION_INFOW> session_info;
    DWORD count = 0;

    if (!WTSEnumerateSessionsW(WTS_CURRENT_SERVER_HANDLE, 0, 1, session_info.Recieve(), &count))
    {
        DPLOG(LS_WARNING) << "WTSEnumerateSessionsW failed";
        return std::string();
    }

    proto::Session message;

    for (DWORD i = 0; i < count; ++i)
    {
        if (session_info[i]->SessionId == 0)
            continue;

        proto::Session::Item* item = message.add_item();

        ScopedWtsMemory<wchar_t*> string_buffer;
        DWORD bytes_returned;

        if (WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE,
                                        session_info[i]->SessionId,
                                        WTSUserName,
                                        string_buffer.Recieve(),
                                        &bytes_returned))
        {
            item->set_user_name(UTF8fromUNICODE(string_buffer));
        }

        if (WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE,
                                        session_info[i]->SessionId,
                                        WTSDomainName,
                                        string_buffer.Recieve(),
                                        &bytes_returned))
        {
            item->set_domain_name(UTF8fromUNICODE(string_buffer));
        }

        item->set_session_id(session_info[i]->SessionId);

        ScopedWtsMemory<int*> int_buffer;

        if (WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE,
                                        session_info[i]->SessionId,
                                        WTSConnectState,
                                        reinterpret_cast<LPWSTR*>(int_buffer.Recieve()),
                                        &bytes_returned))
        {
            switch (*int_buffer)
            {
                case WTSActive:
                    item->set_connect_state(proto::Session::CONNECT_STATE_ACTIVE);
                    break;

                case WTSConnected:
                    item->set_connect_state(proto::Session::CONNECT_STATE_CONNECTED);
                    break;

                case WTSConnectQuery:
                    item->set_connect_state(proto::Session::CONNECT_STATE_CONNECT_QUERY);
                    break;

                case WTSShadow:
                    item->set_connect_state(proto::Session::CONNECT_STATE_SHADOW);
                    break;

                case WTSDisconnected:
                    item->set_connect_state(proto::Session::CONNECT_STATE_DISCONNECTED);
                    break;

                case WTSIdle:
                    item->set_connect_state(proto::Session::CONNECT_STATE_IDLE);
                    break;

                case WTSListen:
                    item->set_connect_state(proto::Session::CONNECT_STATE_LISTEN);
                    break;

                case WTSReset:
                    item->set_connect_state(proto::Session::CONNECT_STATE_RESET);
                    break;

                case WTSDown:
                    item->set_connect_state(proto::Session::CONNECT_STATE_DOWN);
                    break;

                case WTSInit:
                    item->set_connect_state(proto::Session::CONNECT_STATE_INIT);
                    break;

                default:
                    break;
            }
        }

        if (WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE,
                                        session_info[i]->SessionId,
                                        WTSClientName,
                                        string_buffer.Recieve(),
                                        &bytes_returned))
        {
            item->set_client_name(UTF8fromUNICODE(string_buffer));
        }

        if (WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE,
                                        session_info[i]->SessionId,
                                        WTSWinStationName,
                                        string_buffer.Recieve(),
                                        &bytes_returned))
        {
            item->set_winstation_name(UTF8fromUNICODE(string_buffer));
        }
    }

    return message.SerializeAsString();
}

} // namespace aspia
