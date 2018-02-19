//
// PROJECT:         Aspia
// FILE:            category/category_eventlog.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/strings/string_printf.h"
#include "base/strings/unicode.h"
#include "base/datetime.h"
#include "base/logging.h"
#include "base/registry.h"
#include "base/scoped_object.h"
#include "category/category_eventlog.h"
#include "category/category_eventlog.pb.h"
#include "ui/resource.h"

#include <memory>
#include <vector>
#include <strsafe.h>

namespace aspia {

namespace {

constexpr WORD kAllowedEventTypes =
    EVENTLOG_ERROR_TYPE | EVENTLOG_WARNING_TYPE | EVENTLOG_AUDIT_FAILURE;

HANDLE OpenEventLogHandle(const wchar_t* source, DWORD* records_count, DWORD* first_record)
{
    ScopedEventLog event_log(OpenEventLogW(nullptr, source));
    if (!event_log.IsValid())
    {
        DPLOG(LS_WARNING) << "OpenEventLogW failed";
        return nullptr;
    }

    if (!GetNumberOfEventLogRecords(event_log, records_count))
    {
        DPLOG(LS_WARNING) << "GetNumberOfEventLogRecords failed";
        return nullptr;
    }

    if (!GetOldestEventLogRecord(event_log, first_record))
    {
        DPLOG(LS_WARNING) << "GetOldestEventLogRecord failed";
        return nullptr;
    }

    return event_log.Release();
}

std::unique_ptr<uint8_t[]> GetEventLogRecord(
    HANDLE event_log, DWORD record_offset, EVENTLOGRECORD** record)
{
    std::unique_ptr<uint8_t[]> record_buffer =
        std::make_unique<uint8_t[]>(sizeof(EVENTLOGRECORD));

    *record = reinterpret_cast<EVENTLOGRECORD*>(record_buffer.get());

    DWORD bytes_read = 0;
    DWORD bytes_needed = 0;

    if (!ReadEventLogW(event_log,
                       EVENTLOG_SEEK_READ | EVENTLOG_FORWARDS_READ,
                       record_offset,
                       *record,
                       sizeof(EVENTLOGRECORD),
                       &bytes_read,
                       &bytes_needed))
    {
        DWORD error_code = GetLastError();

        if (error_code != ERROR_INSUFFICIENT_BUFFER)
        {
            DLOG(LS_WARNING) << "ReadEventLogW failed: " << SystemErrorCodeToString(error_code);
            return nullptr;
        }

        record_buffer = std::make_unique<uint8_t[]>(bytes_needed);
        *record = reinterpret_cast<EVENTLOGRECORD*>(record_buffer.get());

        if (!ReadEventLogW(event_log,
                           EVENTLOG_SEEK_READ | EVENTLOG_FORWARDS_READ,
                           record_offset,
                           *record,
                           bytes_needed,
                           &bytes_read,
                           &bytes_needed))
        {
            DPLOG(LS_WARNING) << "ReadEventLogW failed";
            return nullptr;
        }
    }

    return record_buffer;
}

bool GetEventLogMessageFileDLL(
    const wchar_t* log_name, const wchar_t* source, std::wstring* message_file)
{
    wchar_t key_path[MAX_PATH];

    HRESULT hr = StringCbPrintfW(key_path, sizeof(key_path),
                                 L"SYSTEM\\CurrentControlSet\\Services\\EventLog\\%s\\%s",
                                 log_name, source);
    if (FAILED(hr))
    {
        DLOG(LS_WARNING) << "StringCbPrintfW failed: " << SystemErrorCodeToString(hr);
        return false;
    }

    RegistryKey key;

    LONG status = key.Open(HKEY_LOCAL_MACHINE, key_path, KEY_READ);
    if (status != ERROR_SUCCESS)
    {
        DLOG(LS_WARNING) << "key.Open failed: " << SystemErrorCodeToString(status);
        return false;
    }

    status = key.ReadValue(L"EventMessageFile", message_file);
    if (status != ERROR_SUCCESS)
    {
        DLOG(LS_WARNING) << "key.ReadValue failed: " << SystemErrorCodeToString(status);
        return false;
    }

    return true;
}

wchar_t* LoadMessageFromDLL(const wchar_t* module_name, DWORD event_id, wchar_t** arguments)
{
    HINSTANCE module = LoadLibraryExW(module_name, nullptr,
                                      DONT_RESOLVE_DLL_REFERENCES |
                                      LOAD_LIBRARY_AS_DATAFILE);
    if (!module)
        return nullptr;

    DWORD flags = FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER |
                  FORMAT_MESSAGE_FROM_HMODULE | FORMAT_MESSAGE_ARGUMENT_ARRAY |
                  FORMAT_MESSAGE_MAX_WIDTH_MASK;

    wchar_t* message_buffer = nullptr;
    DWORD length = 0;

    __try
    {
        // SEH to protect from invalid string parameters.
        __try
        {
            length = FormatMessageW(flags,
                                    module,
                                    event_id,
                                    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                                    reinterpret_cast<LPWSTR>(&message_buffer),
                                    0,
                                    reinterpret_cast<va_list*>(arguments));
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            flags &= ~FORMAT_MESSAGE_ARGUMENT_ARRAY;
            flags |= FORMAT_MESSAGE_IGNORE_INSERTS;

            length = FormatMessageW(flags,
                                    module,
                                    event_id,
                                    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                                    reinterpret_cast<LPWSTR>(&message_buffer),
                                    0,
                                    nullptr);
        }
    }
    __finally
    {
        FreeLibrary(module);
    }

    if (!length)
        return nullptr;

    return message_buffer;
}

bool GetEventLogMessage(const wchar_t* log_name, EVENTLOGRECORD* record, std::wstring* message)
{
    wchar_t* source = reinterpret_cast<wchar_t*>(record + 1);

    std::wstring message_file;

    if (!GetEventLogMessageFileDLL(log_name, source, &message_file))
        return false;

    wchar_t* argument = reinterpret_cast<wchar_t*>(
        reinterpret_cast<LPBYTE>(record) + record->StringOffset);

    std::vector<wchar_t*> arguments;
    arguments.resize(record->NumStrings);

    for (WORD i = 0; i < record->NumStrings; ++i)
    {
        arguments[i] = argument;
        argument += lstrlenW(argument) + 1;
    }

    wchar_t* file = &message_file[0];

    while (file)
    {
        wchar_t* next_file = wcschr(file, L';');

        if (next_file != nullptr)
        {
            *next_file = 0;
            ++next_file;
        }

        wchar_t module_name[MAX_PATH];

        if (ExpandEnvironmentStringsW(file, module_name, _countof(module_name)) != 0)
        {
            wchar_t* message_buffer =
                LoadMessageFromDLL(module_name, record->EventID, arguments.data());

            if (message_buffer)
            {
                message->assign(message_buffer);
                LocalFree(message_buffer);
                return true;
            }
        }

        file = next_file;
    }

    return false;
}

void AddEventLogItems(const wchar_t* log_name, proto::EventLog::Log* log)
{
    DWORD records_count = 0;
    DWORD first_record = 0;

    ScopedEventLog event_log(OpenEventLogHandle(log_name, &records_count, &first_record));
    if (!event_log.IsValid())
        return;

    DWORD end_record = first_record + records_count;

    for (DWORD i = first_record; i < end_record; ++i)
    {
        EVENTLOGRECORD* record;

        std::unique_ptr<uint8_t[]> record_buffer = GetEventLogRecord(event_log, i, &record);
        if (record_buffer)
        {
            if (!(kAllowedEventTypes & record->EventType))
                continue;

            proto::EventLog::Log::Item* item = log->add_item();

            switch (record->EventType)
            {
                case EVENTLOG_ERROR_TYPE:
                    item->set_level(proto::EventLog::Log::LEVEL_ERROR);
                    break;

                case EVENTLOG_WARNING_TYPE:
                    item->set_level(proto::EventLog::Log::LEVEL_WARNING);
                    break;

                case EVENTLOG_INFORMATION_TYPE:
                    item->set_level(proto::EventLog::Log::LEVEL_INFORMATION);
                    break;

                case EVENTLOG_AUDIT_SUCCESS:
                    item->set_level(proto::EventLog::Log::LEVEL_AUDIT_SUCCESS);
                    break;

                case EVENTLOG_AUDIT_FAILURE:
                    item->set_level(proto::EventLog::Log::LEVEL_AUDIT_FAILURE);
                    break;

                case EVENTLOG_SUCCESS:
                    item->set_level(proto::EventLog::Log::LEVEL_SUCCESS);
                    break;

                default:
                    DLOG(LS_WARNING) << "Unknown event type: " << record->EventType;
                    break;
            }

            item->set_time(record->TimeGenerated);
            item->set_category(record->EventCategory);
            item->set_event_id(record->EventID & 0xFFFF);
            item->set_source(UTF8fromUNICODE(reinterpret_cast<wchar_t*>(record + 1)));

            std::wstring description;

            if (GetEventLogMessage(log_name, record, &description))
                item->set_description(UTF8fromUNICODE(description));
        }
    }
}

const char* TypeToString(proto::EventLog::Log::Type value)
{
    switch (value)
    {
        case proto::EventLog::Log::TYPE_APPLICATION:
            return "Application";

        case proto::EventLog::Log::TYPE_SECURITY:
            return "Security";

        case proto::EventLog::Log::TYPE_SYSTEM:
            return "System";

        default:
            return "Unknown";
    }
}

const char* LevelToString(proto::EventLog::Log::Level value)
{
    switch (value)
    {
        case proto::EventLog::Log::LEVEL_INFORMATION:
            return "Information";

        case proto::EventLog::Log::LEVEL_WARNING:
            return "Warning";

        case proto::EventLog::Log::LEVEL_ERROR:
            return "Error";

        case proto::EventLog::Log::LEVEL_AUDIT_SUCCESS:
            return "Audit Success";

        case proto::EventLog::Log::LEVEL_AUDIT_FAILURE:
            return "Audit Failure";

        default:
            return "Unknown";
    }
}

} // namespace

CategoryEventLog::CategoryEventLog()
    : CategoryInfo(Type::INFO_LIST)
{
    // Nothing
}

const char* CategoryEventLog::Name() const
{
    return "Event Logs";
}

Category::IconId CategoryEventLog::Icon() const
{
    return IDI_BOOKS_STACK;
}

const char* CategoryEventLog::Guid() const
{
    return "{0DD03A20-D1AF-4D1F-938F-956EE9003EE9}";
}

void CategoryEventLog::Parse(Table& table, const std::string& data)
{
    proto::EventLog message;

    if (!message.ParseFromString(data))
        return;

    table.AddColumns(ColumnList::Create()
                     .AddColumn("Type", 100)
                     .AddColumn("Level", 80)
                     .AddColumn("Time", 120)
                     .AddColumn("Category", 60)
                     .AddColumn("Event Id", 60)
                     .AddColumn("Source", 140)
                     .AddColumn("Description", 180));

    for (int index = 0; index < message.log_size(); ++index)
    {
        const proto::EventLog::Log& log = message.log(index);

        for (int i = 0; i < log.item_size(); ++i)
        {
            const proto::EventLog::Log::Item& item = log.item(i);

            Row row = table.AddRow();
            row.AddValue(Value::String(TypeToString(log.type())));
            row.AddValue(Value::String(LevelToString(item.level())));
            row.AddValue(Value::String(TimeToString(item.time())));
            row.AddValue(Value::Number(item.category()));
            row.AddValue(Value::Number(item.event_id()));
            row.AddValue(Value::String(item.source()));
            row.AddValue(Value::String(item.description()));
        }
    }
}

std::string CategoryEventLog::Serialize()
{
    struct Logs
    {
        const wchar_t* name;
        proto::EventLog::Log::Type type;
    } static const kLogs[] =
    {
        { L"Application", proto::EventLog::Log::TYPE_APPLICATION },
        { L"System", proto::EventLog::Log::TYPE_SYSTEM },
        { L"Security", proto::EventLog::Log::TYPE_SECURITY }
    };

    proto::EventLog message;

    for (size_t i = 0; i < _countof(kLogs); ++i)
    {
        proto::EventLog::Log* log = message.add_log();
        log->set_type(kLogs[i].type);
        AddEventLogItems(kLogs[i].name, log);
    }

    return message.SerializeAsString();
}

} // namespace aspia
