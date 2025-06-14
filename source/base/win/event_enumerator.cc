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

#include "base/win/event_enumerator.h"

#include "base/logging.h"
#include "base/win/registry.h"

#include <strsafe.h>

namespace base {

namespace {

//--------------------------------------------------------------------------------------------------
void resizeBuffer(QByteArray* buffer, size_t size)
{
    if (buffer->capacity() < static_cast<QByteArray::size_type>(size))
        buffer->reserve(static_cast<QByteArray::size_type>(size));

    buffer->resize(static_cast<QByteArray::size_type>(size));
}

//--------------------------------------------------------------------------------------------------
HANDLE openEventLogHandle(const wchar_t* source, DWORD* records_count, DWORD* first_record)
{
    ScopedEventLog event_log(OpenEventLogW(nullptr, source));
    if (!event_log.isValid())
    {
        PLOG(ERROR) << "OpenEventLogW failed";
        return nullptr;
    }

    if (!GetNumberOfEventLogRecords(event_log.get(), records_count))
    {
        PLOG(ERROR) << "GetNumberOfEventLogRecords failed";
        return nullptr;
    }

    if (!GetOldestEventLogRecord(event_log.get(), first_record))
    {
        PLOG(ERROR) << "GetOldestEventLogRecord failed";
        return nullptr;
    }

    return event_log.release();
}

//--------------------------------------------------------------------------------------------------
bool eventLogRecord(HANDLE event_log, DWORD record_offset, QByteArray* record_buffer)
{
    resizeBuffer(record_buffer, sizeof(EVENTLOGRECORD));

    EVENTLOGRECORD* record = reinterpret_cast<EVENTLOGRECORD*>(record_buffer->data());

    DWORD bytes_read = 0;
    DWORD bytes_needed = 0;

    if (!ReadEventLogW(event_log,
                       EVENTLOG_SEEK_READ | EVENTLOG_BACKWARDS_READ,
                       record_offset,
                       record,
                       sizeof(EVENTLOGRECORD),
                       &bytes_read,
                       &bytes_needed))
    {
        DWORD error_code = GetLastError();

        if (error_code != ERROR_INSUFFICIENT_BUFFER)
        {
            LOG(ERROR) << "ReadEventLogW failed:" << SystemError(error_code).toString();
            return false;
        }

        resizeBuffer(record_buffer, bytes_needed);
        record = reinterpret_cast<EVENTLOGRECORD*>(record_buffer->data());

        if (!ReadEventLogW(event_log,
                           EVENTLOG_SEEK_READ | EVENTLOG_BACKWARDS_READ,
                           record_offset,
                           record,
                           bytes_needed,
                           &bytes_read,
                           &bytes_needed))
        {
            PLOG(ERROR) << "ReadEventLogW failed";
            return false;
        }
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool eventLogMessageFileDLL(
    const QString& log_name, const QString& source, std::wstring* message_file)
{
    QString key_path = QString("SYSTEM\\CurrentControlSet\\Services\\EventLog\\%1\\%2")
        .arg(log_name, source);

    RegistryKey key;

    LONG status = key.open(HKEY_LOCAL_MACHINE, key_path, KEY_READ);
    if (status != ERROR_SUCCESS)
    {
        LOG(ERROR) << "key.open failed:" << SystemError(static_cast<DWORD>(status)).toString();
        return false;
    }

    status = key.readValue("EventMessageFile", message_file);
    if (status != ERROR_SUCCESS)
    {
        LOG(INFO) << "key.readValue failed:" << SystemError(static_cast<DWORD>(status)).toString();
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
wchar_t* loadMessageFromDLL(const wchar_t* module_name, DWORD event_id, wchar_t** arguments)
{
    HINSTANCE module = LoadLibraryExW(module_name,
                                      nullptr,
                                      DONT_RESOLVE_DLL_REFERENCES | LOAD_LIBRARY_AS_DATAFILE);
    if (!module)
        return nullptr;

    DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_HMODULE |
                  FORMAT_MESSAGE_ARGUMENT_ARRAY | FORMAT_MESSAGE_MAX_WIDTH_MASK;

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
                                    MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL),
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
                                    MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL),
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

//--------------------------------------------------------------------------------------------------
bool eventLogMessage(const QString& log_name, EVENTLOGRECORD* record, QString* message)
{
    QString source = QString::fromWCharArray(reinterpret_cast<wchar_t*>(record + 1));

    std::wstring message_file;

    if (!eventLogMessageFileDLL(log_name, source, &message_file))
        return false;

    wchar_t* argument = reinterpret_cast<wchar_t*>(
        reinterpret_cast<LPBYTE>(record) + record->StringOffset);

    static const WORD kMaxInsertStrings = 100;

    wchar_t* arguments[kMaxInsertStrings];
    memset(arguments, 0, sizeof(arguments));

    WORD num_strings = std::min(record->NumStrings, kMaxInsertStrings);
    for (WORD i = 0; i < num_strings; ++i)
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
                loadMessageFromDLL(module_name, record->EventID, arguments);

            if (message_buffer)
            {
                *message = QString::fromWCharArray(message_buffer);
                LocalFree(message_buffer);
                return true;
            }
        }

        file = next_file;
    }

    if (num_strings > 0)
    {
        argument = reinterpret_cast<wchar_t*>(
           reinterpret_cast<LPBYTE>(record) + record->StringOffset);

        for (int i = 0; i < num_strings; ++i)
        {
            message->append(argument);

            if (i != num_strings - 1)
                message->append(L"; ");

            argument += lstrlenW(argument) + 1;
        }

        return true;
    }

    return false;
}

} // namespace

//--------------------------------------------------------------------------------------------------
EventEnumerator::EventEnumerator(const QString& log_name, quint32 start, quint32 count)
    : log_name_(log_name)
{
    if (!count)
        return;

    DWORD first_record = 0;
    DWORD records_count = 0;

    event_log_.reset(openEventLogHandle(qUtf16Printable(log_name_), &records_count, &first_record));
    if (!records_count)
        return;

    records_count_ = records_count;

    int end = static_cast<int>(first_record + records_count);

    current_pos_ = end - static_cast<int>(start) - 1;
    if (current_pos_ < static_cast<int>(first_record))
        current_pos_ = static_cast<int>(first_record);
    else if (current_pos_ > end - 1)
        current_pos_ = end - 1;

    end_record_ = current_pos_ - static_cast<int>(count) + 1;
    if (end_record_ < static_cast<int>(first_record))
        end_record_ = static_cast<int>(first_record);

    LOG(INFO) << "Log name:" << log_name_;
    LOG(INFO) << "First:" << first_record << "count:" << records_count
              << "pos:" << current_pos_ << "end:" << end_record_;
}

//--------------------------------------------------------------------------------------------------
EventEnumerator::~EventEnumerator() = default;

//--------------------------------------------------------------------------------------------------
quint32 EventEnumerator::count() const
{
    return records_count_;
}

//--------------------------------------------------------------------------------------------------
bool EventEnumerator::isAtEnd() const
{
    if (!event_log_.isValid())
        return true;

    for (;;)
    {
        if (current_pos_ < end_record_)
            return true;

        if (eventLogRecord(event_log_.get(), static_cast<quint32>(current_pos_), &record_buffer_))
            return false;

        record_buffer_.clear();
        --current_pos_;
    }
}

//--------------------------------------------------------------------------------------------------
void EventEnumerator::advance()
{
    record_buffer_.clear();
    --current_pos_;
}

//--------------------------------------------------------------------------------------------------
EventEnumerator::Type EventEnumerator::type() const
{
    switch (record()->EventType)
    {
        case EVENTLOG_ERROR_TYPE:
            return Type::ERR;

        case EVENTLOG_WARNING_TYPE:
            return Type::WARN;

        case EVENTLOG_INFORMATION_TYPE:
            return Type::INFO;

        case EVENTLOG_AUDIT_SUCCESS:
            return Type::AUDIT_SUCCESS;

        case EVENTLOG_AUDIT_FAILURE:
            return Type::AUDIT_FAILURE;

        case EVENTLOG_SUCCESS:
            return Type::SUCCESS;

        default:
            return Type::UNKNOWN;
    }
}

//--------------------------------------------------------------------------------------------------
qint64 EventEnumerator::time() const
{
    return record()->TimeGenerated;
}

//--------------------------------------------------------------------------------------------------
QString EventEnumerator::category() const
{
    return QString::number(record()->EventCategory);
}

//--------------------------------------------------------------------------------------------------
quint32 EventEnumerator::eventId() const
{
    return record()->EventID & 0xFFFF;
}

//--------------------------------------------------------------------------------------------------
QString EventEnumerator::source() const
{
    return QString::fromWCharArray(reinterpret_cast<wchar_t*>(record() + 1));
}

//--------------------------------------------------------------------------------------------------
QString EventEnumerator::description() const
{
    QString desc_wide;
    if (!eventLogMessage(log_name_, record(), &desc_wide))
        return QString();

    return desc_wide;
}

//--------------------------------------------------------------------------------------------------
EVENTLOGRECORD* EventEnumerator::record() const
{
    return reinterpret_cast<EVENTLOGRECORD*>(record_buffer_.data());
}

} // namespace base
