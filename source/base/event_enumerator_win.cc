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

#include <QStringList>

#include <winmeta.h>

#include <string>

#include "base/logging.h"

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
bool hasValue(const EVT_VARIANT& value)
{
    return (value.Type & EVT_VARIANT_TYPE_MASK) != EvtVarTypeNull;
}

//--------------------------------------------------------------------------------------------------
quint32 logRecordCount(const QString& log_name)
{
    ScopedEvtHandle log(EvtOpenLog(nullptr, qUtf16Printable(log_name), EvtOpenChannelPath));
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

//--------------------------------------------------------------------------------------------------
QString formatEventMessage(EVT_HANDLE metadata, EVT_HANDLE event, EVT_FORMAT_MESSAGE_FLAGS flag)
{
    DWORD buffer_used = 0;

    // First call to determine the required buffer size.
    EvtFormatMessage(metadata, event, 0, 0, nullptr, flag, 0, nullptr, &buffer_used);
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || buffer_used == 0)
        return QString();

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
            return QString();
        }
    }

    return QString::fromWCharArray(buffer.c_str());
}

} // namespace

//--------------------------------------------------------------------------------------------------
EventEnumeratorWin::EventEnumeratorWin(const QString& log_name, quint32 start, quint32 count)
    : log_name_(log_name)
{
    if (!count)
        return;

    records_count_ = logRecordCount(log_name_);

    render_context_.reset(EvtCreateRenderContext(0, nullptr, EvtRenderContextSystem));
    if (!render_context_.isValid())
    {
        PLOG(ERROR) << "EvtCreateRenderContext failed";
        return;
    }

    // Reverse direction makes the newest records come first in the result set.
    query_.reset(EvtQuery(nullptr, qUtf16Printable(log_name_), nullptr,
                          EvtQueryChannelPath | EvtQueryReverseDirection));
    if (!query_.isValid())
    {
        PLOG(ERROR) << "EvtQuery failed";
        return;
    }

    if (start > 0)
    {
        if (!EvtSeek(query_.get(), static_cast<LONGLONG>(start), nullptr, 0, EvtSeekRelativeToFirst))
        {
            PLOG(ERROR) << "EvtSeek failed";
        }
    }

    remaining_ = static_cast<int>(count);

    LOG(TRACE) << "Log name:" << log_name_;
    LOG(TRACE) << "Total:" << records_count_ << "start:" << start << "count:" << count;
}

//--------------------------------------------------------------------------------------------------
EventEnumeratorWin::~EventEnumeratorWin() = default;

//--------------------------------------------------------------------------------------------------
quint32 EventEnumeratorWin::count() const
{
    return records_count_;
}

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
QString EventEnumeratorWin::category() const
{
    EVT_VARIANT* values = systemValues(values_buffer_);
    quint16 task = hasValue(values[EvtSystemTask]) ? values[EvtSystemTask].UInt16Val : 0;
    return QString::number(task);
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
QString EventEnumeratorWin::source() const
{
    EVT_VARIANT* values = systemValues(values_buffer_);
    if (!hasValue(values[EvtSystemProviderName]) || !values[EvtSystemProviderName].StringVal)
        return QString();

    return QString::fromWCharArray(values[EvtSystemProviderName].StringVal);
}

//--------------------------------------------------------------------------------------------------
QString EventEnumeratorWin::description() const
{
    if (!event_ready_)
        return QString();

    EVT_VARIANT* values = systemValues(values_buffer_);
    const wchar_t* provider = hasValue(values[EvtSystemProviderName]) ?
        values[EvtSystemProviderName].StringVal : nullptr;

    ScopedEvtHandle metadata(EvtOpenPublisherMetadata(nullptr, provider, nullptr, 0, 0));

    QString message = formatEventMessage(metadata.get(), event_.get(), EvtFormatMessageEvent);
    if (!message.isEmpty())
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
QString EventEnumeratorWin::eventDataString() const
{
    ScopedEvtHandle context(EvtCreateRenderContext(0, nullptr, EvtRenderContextUser));
    if (!context.isValid())
        return QString();

    DWORD buffer_used = 0;
    DWORD property_count = 0;

    EvtRender(context.get(), event_.get(), EvtRenderEventValues, 0, nullptr, &buffer_used, &property_count);
    if (buffer_used == 0)
        return QString();

    QByteArray buffer;
    resizeBuffer(&buffer, buffer_used);

    if (!EvtRender(context.get(), event_.get(), EvtRenderEventValues,
                   static_cast<DWORD>(buffer.size()), buffer.data(),
                   &buffer_used, &property_count))
    {
        return QString();
    }

    EVT_VARIANT* values = systemValues(buffer);

    QStringList strings;
    for (DWORD i = 0; i < property_count; ++i)
    {
        if ((values[i].Type & EVT_VARIANT_TYPE_MASK) == EvtVarTypeString && values[i].StringVal)
            strings.append(QString::fromWCharArray(values[i].StringVal));
    }

    return strings.join("; ");
}
