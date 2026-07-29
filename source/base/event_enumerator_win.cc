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

#include "base/event_enumerator_win.h"

#include <winmeta.h>

#include <charconv>
#include <vector>

#include "base/logging.h"
#include "base/string_util.h"

namespace {

//--------------------------------------------------------------------------------------------------
void resizeBuffer(QByteArray* buffer, size_t size)
{
    if (buffer->capacity() < static_cast<qsizetype>(size))
        buffer->reserve(static_cast<qsizetype>(size));

    buffer->resize(static_cast<qsizetype>(size));
}

//--------------------------------------------------------------------------------------------------
EVT_VARIANT* systemValues(const QByteArray& buffer)
{
    return reinterpret_cast<EVT_VARIANT*>(const_cast<char*>(buffer.constData()));
}

//--------------------------------------------------------------------------------------------------
// The API answers in UTF-16, the rest of the code works in UTF-8. A length of -1 leaves finding the
// end of the string to the conversion, which walks it anyway; the count it answers with then counts
// the terminator, which the result does not keep.
std::string utf8FromWide(const wchar_t* string)
{
    if (!string || !*string)
        return std::string();

    const int size = WideCharToMultiByte(CP_UTF8, 0, string, -1, nullptr, 0, nullptr, nullptr);
    if (size <= 0)
    {
        PLOG(ERROR) << "WideCharToMultiByte failed";
        return std::string();
    }

    std::string result(static_cast<size_t>(size), '\0');

    // The measured size is what the conversion has to write: anything else is a failure, and the
    // buffer is then left holding whatever part of the string was converted.
    if (WideCharToMultiByte(CP_UTF8, 0, string, -1, result.data(), size, nullptr, nullptr) != size)
    {
        PLOG(ERROR) << "WideCharToMultiByte failed";
        return std::string();
    }

    result.resize(static_cast<size_t>(size) - 1);
    return result;
}

//--------------------------------------------------------------------------------------------------
std::wstring wideFromUtf8(std::string_view string)
{
    const int length = static_cast<int>(string.size());
    if (!length)
        return std::wstring();

    const int size = MultiByteToWideChar(CP_UTF8, 0, string.data(), length, nullptr, 0);
    if (size <= 0)
    {
        PLOG(ERROR) << "MultiByteToWideChar failed";
        return std::wstring();
    }

    std::wstring result(static_cast<size_t>(size), L'\0');

    if (MultiByteToWideChar(CP_UTF8, 0, string.data(), length, result.data(), size) != size)
    {
        PLOG(ERROR) << "MultiByteToWideChar failed";
        return std::wstring();
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
bool hasValue(const EVT_VARIANT& value)
{
    return (value.Type & EVT_VARIANT_TYPE_MASK) != EvtVarTypeNull;
}

//--------------------------------------------------------------------------------------------------
std::string formatEventMessage(EVT_HANDLE metadata, EVT_HANDLE event, EVT_FORMAT_MESSAGE_FLAGS flag)
{
    DWORD buffer_used = 0;

    // First call to determine the required buffer size.
    EvtFormatMessage(metadata, event, 0, 0, nullptr, flag, 0, nullptr, &buffer_used);
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || buffer_used == 0)
        return std::string();

    std::wstring buffer;
    buffer.resize(buffer_used);

    if (!EvtFormatMessage(metadata, event, 0, 0, nullptr, flag, buffer_used, buffer.data(), &buffer_used))
    {
        DWORD error_code = GetLastError();

        // The message text is still rendered even when some inserts can not be resolved.
        if (error_code != ERROR_EVT_UNRESOLVED_VALUE_INSERT &&
            error_code != ERROR_EVT_UNRESOLVED_PARAMETER_INSERT &&
            error_code != ERROR_EVT_MAX_INSERTS_REACHED)
        {
            return std::string();
        }
    }

    return utf8FromWide(buffer.c_str());
}

//--------------------------------------------------------------------------------------------------
// Returns the total number of records in the log via an O(1) metadata query.
quint32 logRecordCount(const std::wstring& log_name)
{
    ScopedEvtHandle log(EvtOpenLog(nullptr, log_name.c_str(), EvtOpenChannelPath));
    if (!log.isValid())
    {
        PLOG(ERROR) << "EvtOpenLog failed";
        return 0;
    }

    EVT_VARIANT value;
    DWORD buffer_used = 0;

    if (!EvtGetLogInfo(log.get(), EvtLogNumberOfLogRecords, sizeof(value), &value, &buffer_used))
    {
        PLOG(ERROR) << "EvtGetLogInfo failed";
        return 0;
    }

    return static_cast<quint32>(value.UInt64Val);
}

} // namespace

//--------------------------------------------------------------------------------------------------
EventEnumeratorWin::EventEnumeratorWin(std::string_view log_name, std::string_view cursor,
                                       Direction direction, quint32 count)
    : log_name_(wideFromUtf8(log_name)),
      count_(count)
{
    if (!count)
        return;

    // The opaque cursor carries the record offset of the page boundary; compute this page's start.
    if (!cursor.empty())
    {
        quint32 offset = 0;
        std::from_chars(cursor.data(), cursor.data() + cursor.size(), offset);
        if (direction == Direction::OLDER)
            start_ = offset + 1;
        else if (offset > count)
            start_ = offset - count;
        // Otherwise NEWER navigation reached the newest records and start_ stays 0.
    }
    else if (direction == Direction::NEWER)
    {
        // Empty cursor + NEWER jumps to the oldest page, i.e. the last records of the log.
        const quint32 total = logRecordCount(log_name_);
        start_ = (total > count) ? total - count : 0;
        at_oldest_hint_ = true;
    }
    // Otherwise (empty cursor + OLDER) this is the newest page and start_ stays 0.

    render_context_.reset(EvtCreateRenderContext(0, nullptr, EvtRenderContextSystem));
    if (!render_context_.isValid())
    {
        PLOG(ERROR) << "EvtCreateRenderContext failed";
        return;
    }

    // Reverse direction makes the newest records come first in the result set.
    query_.reset(EvtQuery(nullptr, log_name_.c_str(), nullptr,
                          EvtQueryChannelPath | EvtQueryReverseDirection));
    if (!query_.isValid())
    {
        PLOG(ERROR) << "EvtQuery failed";
        return;
    }

    if (start_ > 0)
    {
        if (!EvtSeek(query_.get(), static_cast<LONGLONG>(start_), nullptr, 0, EvtSeekRelativeToFirst))
            PLOG(ERROR) << "EvtSeek failed";
    }

    remaining_ = static_cast<int>(count);

    LOG(TRACE) << "Log name:" << log_name_ << "start:" << start_ << "count:" << count;
}

//--------------------------------------------------------------------------------------------------
EventEnumeratorWin::~EventEnumeratorWin() = default;

//--------------------------------------------------------------------------------------------------
bool EventEnumeratorWin::isAtEnd() const
{
    if (!query_.isValid())
        return true;

    if (event_ready_)
        return false;

    return !fetchNext();
}

//--------------------------------------------------------------------------------------------------
void EventEnumeratorWin::advance()
{
    event_.reset();
    event_ready_ = false;
    --remaining_;
}

//--------------------------------------------------------------------------------------------------
std::string EventEnumeratorWin::firstCursor() const
{
    if (read_count_ == 0)
        return std::string();

    return std::to_string(start_);
}

//--------------------------------------------------------------------------------------------------
std::string EventEnumeratorWin::lastCursor() const
{
    if (read_count_ == 0)
        return std::string();

    return std::to_string(start_ + read_count_ - 1);
}

//--------------------------------------------------------------------------------------------------
bool EventEnumeratorWin::atNewest() const
{
    return start_ == 0;
}

//--------------------------------------------------------------------------------------------------
bool EventEnumeratorWin::atOldest() const
{
    return at_oldest_hint_ || read_count_ < count_;
}

//--------------------------------------------------------------------------------------------------
EventEnumerator::Type EventEnumeratorWin::type() const
{
    EVT_VARIANT* values = systemValues(values_buffer_);

    quint64 keywords = hasValue(values[EvtSystemKeywords]) ? values[EvtSystemKeywords].UInt64Val : 0;

    if (keywords & 0x0020000000000000ULL) // WINEVENT_KEYWORD_AUDIT_SUCCESS.
        return Type::AUDIT_SUCCESS;

    if (keywords & 0x0010000000000000ULL) // WINEVENT_KEYWORD_AUDIT_FAILURE.
        return Type::AUDIT_FAILURE;

    quint8 level = hasValue(values[EvtSystemLevel]) ? values[EvtSystemLevel].ByteVal : 0;
    switch (level)
    {
        case WINEVENT_LEVEL_CRITICAL:
        case WINEVENT_LEVEL_ERROR:
            return Type::ERR;

        case WINEVENT_LEVEL_WARNING:
            return Type::WARN;

        default:
            return Type::INFO;
    }
}

//--------------------------------------------------------------------------------------------------
qint64 EventEnumeratorWin::time() const
{
    EVT_VARIANT* values = systemValues(values_buffer_);
    if (!hasValue(values[EvtSystemTimeCreated]))
        return 0;

    // Convert the FILETIME value (100-ns intervals since 1601) to seconds since the Unix epoch.
    static const quint64 kUnixEpochOffset = 116444736000000000ULL;
    quint64 file_time = values[EvtSystemTimeCreated].FileTimeVal;
    if (file_time < kUnixEpochOffset)
        return 0;

    return static_cast<qint64>((file_time - kUnixEpochOffset) / 10000000ULL);
}

//--------------------------------------------------------------------------------------------------
quint32 EventEnumeratorWin::eventId() const
{
    EVT_VARIANT* values = systemValues(values_buffer_);
    if (!hasValue(values[EvtSystemEventID]))
        return 0;

    return values[EvtSystemEventID].UInt16Val;
}

//--------------------------------------------------------------------------------------------------
std::string EventEnumeratorWin::source() const
{
    EVT_VARIANT* values = systemValues(values_buffer_);
    if (!hasValue(values[EvtSystemProviderName]) || !values[EvtSystemProviderName].StringVal)
        return std::string();

    return utf8FromWide(values[EvtSystemProviderName].StringVal);
}

//--------------------------------------------------------------------------------------------------
std::string EventEnumeratorWin::description() const
{
    if (!event_ready_)
        return std::string();

    EVT_VARIANT* values = systemValues(values_buffer_);
    const wchar_t* provider = hasValue(values[EvtSystemProviderName]) ?
        values[EvtSystemProviderName].StringVal : nullptr;

    ScopedEvtHandle metadata(EvtOpenPublisherMetadata(nullptr, provider, nullptr, 0, 0));

    std::string message = formatEventMessage(metadata.get(), event_.get(), EvtFormatMessageEvent);
    if (!message.empty())
        return message;

    // No message template available, fall back to the raw event data strings.
    return eventDataString();
}

//--------------------------------------------------------------------------------------------------
bool EventEnumeratorWin::fetchNext() const
{
    if (remaining_ <= 0)
        return false;

    for (;;)
    {
        EVT_HANDLE event = nullptr;
        DWORD returned = 0;

        if (!EvtNext(query_.get(), 1, &event, INFINITE, 0, &returned) || returned == 0)
        {
            remaining_ = 0;
            return false;
        }

        event_.reset(event);

        if (renderSystem())
        {
            event_ready_ = true;
            ++read_count_;
            return true;
        }

        // Unable to render the system properties, skip the record.
        event_.reset();
    }
}

//--------------------------------------------------------------------------------------------------
bool EventEnumeratorWin::renderSystem() const
{
    DWORD buffer_used = 0;
    DWORD property_count = 0;

    EvtRender(render_context_.get(), event_.get(), EvtRenderEventValues, 0, nullptr, &buffer_used,
              &property_count);
    if (buffer_used == 0)
        return false;

    resizeBuffer(&values_buffer_, buffer_used);

    if (!EvtRender(render_context_.get(), event_.get(), EvtRenderEventValues,
                   static_cast<DWORD>(values_buffer_.size()), values_buffer_.data(),
                   &buffer_used, &property_count))
    {
        PLOG(ERROR) << "EvtRender failed";
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
std::string EventEnumeratorWin::eventDataString() const
{
    ScopedEvtHandle context(EvtCreateRenderContext(0, nullptr, EvtRenderContextUser));
    if (!context.isValid())
        return std::string();

    DWORD buffer_used = 0;
    DWORD property_count = 0;

    EvtRender(context.get(), event_.get(), EvtRenderEventValues, 0, nullptr, &buffer_used, &property_count);
    if (buffer_used == 0)
        return std::string();

    QByteArray buffer;
    resizeBuffer(&buffer, buffer_used);

    if (!EvtRender(context.get(), event_.get(), EvtRenderEventValues,
                   static_cast<DWORD>(buffer.size()), buffer.data(),
                   &buffer_used, &property_count))
    {
        return std::string();
    }

    EVT_VARIANT* values = systemValues(buffer);

    std::vector<std::string> strings;
    for (DWORD i = 0; i < property_count; ++i)
    {
        if ((values[i].Type & EVT_VARIANT_TYPE_MASK) == EvtVarTypeString && values[i].StringVal)
            strings.emplace_back(utf8FromWide(values[i].StringVal));
    }

    return strJoin(strings, "; ");
}
