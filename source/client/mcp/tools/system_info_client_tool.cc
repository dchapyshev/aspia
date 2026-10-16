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

#include "client/mcp/tools/system_info_client_tool.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QTimer>

#include <algorithm>
#include <optional>

#include "base/time_types.h"
#include "client/mcp/mcp_controller.h"
#include "client/mcp/tools/mcp_utils.h"
#include "common/sys_info/sys_info_report.h"

namespace {

const Seconds kReadTimeout{ 30 };
const int kMaxWait = 60;
const MilliSeconds kWaitInterval{ 100 };
const int kDefaultLimit = 100;
const int kMaxLimit = 1000;

const char kEventLogsPage[] = "event_logs";

struct EventLogName
{
    const char* name;
    proto::system_info::EventLogs::Event::Type type;
};

const EventLogName kEventLogs[] =
{
    { "application", proto::system_info::EventLogs::Event::TYPE_APPLICATION },
    { "security", proto::system_info::EventLogs::Event::TYPE_SECURITY },
    { "system", proto::system_info::EventLogs::Event::TYPE_SYSTEM }
};

struct MoveName
{
    const char* name;
    SysInfoWidgetEventLogs::Move move;
};

const MoveName kMoves[] =
{
    { "newest", SysInfoWidgetEventLogs::Move::NEWEST },
    { "newer", SysInfoWidgetEventLogs::Move::NEWER },
    { "older", SysInfoWidgetEventLogs::Move::OLDER },
    { "oldest", SysInfoWidgetEventLogs::Move::OLDEST }
};

struct ReadOptions
{
    QString page;
    QString filter;
    qint64 offset = 0;
    qint64 limit = kDefaultLimit;
};

//--------------------------------------------------------------------------------------------------
std::optional<SysInfoView::Page> findPage(const McpController::SystemInfoWindow& window, const QString& key)
{
    const QList<SysInfoView::Page> pages = window.pages();
    for (const SysInfoView::Page& page : pages)
    {
        if (page.key == key)
            return page;
    }

    return std::nullopt;
}

//--------------------------------------------------------------------------------------------------
bool isRefreshing(const McpController::SystemInfoWindow& window)
{
    const QList<SysInfoView::Page> pages = window.pages();
    return std::any_of(pages.begin(), pages.end(), [](const SysInfoView::Page& page) { return page.refreshing; });
}

//--------------------------------------------------------------------------------------------------
QString eventLogName(proto::system_info::EventLogs::Event::Type type)
{
    for (const EventLogName& entry : kEventLogs)
    {
        if (entry.type == type)
            return entry.name;
    }

    return "unknown";
}

//--------------------------------------------------------------------------------------------------
QString pagesText(const McpController::SystemInfoWindow& window)
{
    QJsonArray list;

    const QList<SysInfoView::Page> pages = window.pages();
    for (const SysInfoView::Page& page : pages)
    {
        list.append(QJsonObject{ { "page", page.key }, { "name", page.name }, { "group", page.group },
                                 { "has_data", page.has_data }, { "refreshing", page.refreshing } });
    }

    return QString::fromUtf8(QJsonDocument(QJsonObject{ { "pages", list } }).toJson(QJsonDocument::Compact));
}

//--------------------------------------------------------------------------------------------------
QString reportText(McpController::SystemInfoWindow* window, const ReadOptions& options)
{
    SysInfoReport report;
    window->buildReport(&report);

    QJsonArray tables;

    for (const SysInfoReport::Table& table : report.tables())
    {
        const bool nested = std::any_of(table.rows.begin(), table.rows.end(),
                                        [](const SysInfoReport::Row& row) { return row.level > 0; });

        QJsonArray columns;
        if (nested)
            columns.append("level");
        for (const QString& column : table.header)
            columns.append(column);

        QJsonArray rows;
        qint64 total_rows = 0;

        for (const SysInfoReport::Row& row : table.rows)
        {
            const bool matched = options.filter.isEmpty() ||
                std::any_of(row.columns.begin(), row.columns.end(), [&options](const QString& cell)
            {
                return cell.contains(options.filter, Qt::CaseInsensitive);
            });

            if (!matched)
                continue;

            if (total_rows >= options.offset && rows.size() < options.limit)
            {
                QJsonArray cells;
                if (nested)
                    cells.append(row.level);
                for (const QString& cell : row.columns)
                    cells.append(cell);
                rows.append(cells);
            }

            ++total_rows;
        }

        QJsonObject entry{ { "columns", columns }, { "total_rows", total_rows }, { "rows", rows } };
        if (!table.title.isEmpty())
            entry.insert("title", table.title);

        tables.append(entry);
    }

    std::optional<SysInfoView::Page> page = findPage(*window, options.page);

    QJsonObject content{ { "page", options.page }, { "name", page ? page->name : QString() },
                         { "refreshing", page && page->refreshing }, { "tables", tables } };

    if (options.page == kEventLogsPage)
    {
        content.insert("log", eventLogName(window->currentEventLog()));
        content.insert("newer", window->canMoveEventLog(SysInfoWidgetEventLogs::Move::NEWER));
        content.insert("older", window->canMoveEventLog(SysInfoWidgetEventLogs::Move::OLDER));
    }

    return QString::fromUtf8(QJsonDocument(content).toJson(QJsonDocument::Compact));
}

//--------------------------------------------------------------------------------------------------
void waitPage(qint64 client_id, const ReadOptions& options, McpTool::Reply reply)
{
    const TimePoint deadline = Clock::now() + kReadTimeout;

    QTimer* timer = new QTimer(&McpController::instance());
    QObject::connect(timer, &QTimer::timeout, timer, [timer, client_id, options, deadline, reply]()
    {
        McpController::SystemInfoWindow* window = McpController::instance().systemInfoWindow(client_id);
        std::optional<SysInfoView::Page> page = window ? findPage(*window, options.page) : std::nullopt;

        if (page && page->refreshing && Clock::now() < deadline)
            return;

        timer->stop();
        timer->deleteLater();

        if (!window)
        {
            reply(false, QString("The client %1 was closed.").arg(client_id));
            return;
        }

        reply(true, reportText(window, options));
    });
    timer->start(kWaitInterval);
}

//--------------------------------------------------------------------------------------------------
void waitRefresh(qint64 client_id, qint64 wait, McpTool::Reply reply)
{
    const TimePoint deadline = Clock::now() + Seconds(wait);

    QTimer* timer = new QTimer(&McpController::instance());
    QObject::connect(timer, &QTimer::timeout, timer, [timer, client_id, deadline, reply]()
    {
        McpController::SystemInfoWindow* window = McpController::instance().systemInfoWindow(client_id);

        if (window && isRefreshing(*window) && Clock::now() < deadline)
            return;

        timer->stop();
        timer->deleteLater();

        if (!window)
        {
            reply(false, QString("The client %1 was closed.").arg(client_id));
            return;
        }

        reply(true, pagesText(*window));
    });
    timer->start(kWaitInterval);
}

} // namespace

//--------------------------------------------------------------------------------------------------
QString SystemInfoClientTool::name() const
{
    return "system_info_client";
}

//--------------------------------------------------------------------------------------------------
QJsonObject SystemInfoClientTool::definition() const
{
    QJsonObject client_id{ { "type", "integer" },
                           { "description", "Id of a system_info client window from list_clients." } };

    QJsonObject action{ { "type", "string" },
                        { "enum", QJsonArray{ "pages", "read", "refresh" } },
                        { "description", "pages lists the pages of the window. read shows the page in the window "
                                         "as the user does by choosing it, waits while the page is refreshing and "
                                         "returns its tables. refresh asks the host for every page again, as the "
                                         "button Refresh does, and returns the pages." } };

    QJsonObject page{ { "type", "string" },
                      { "description", "Key of the page of read from pages, for example summary, services or "
                                       "event_logs." } };

    QJsonObject filter{ { "type", "string" },
                        { "description", "Text that read looks for in the cells of the rows, ignoring case. Only "
                                         "the rows that have it are returned and counted." } };

    QJsonObject offset{ { "type", "integer" },
                        { "minimum", 0 },
                        { "description", "Number of the rows of each table that read skips. 0 by default." } };

    QJsonObject limit{ { "type", "integer" },
                       { "minimum", 1 },
                       { "maximum", kMaxLimit },
                       { "description", QString("Number of the rows of each table that read returns. %1 by "
                                                "default.").arg(kDefaultLimit) } };

    QJsonObject log{ { "type", "string" },
                     { "enum", QJsonArray{ "application", "security", "system" } },
                     { "description", "Log that read of event_logs chooses in the window. The page then shows "
                                      "its newest events." } };

    QJsonObject move{ { "type", "string" },
                      { "enum", QJsonArray{ "newest", "newer", "older", "oldest" } },
                      { "description", "Page of the log that read of event_logs moves to with the buttons of "
                                       "the window. newer and older are in the result when they are possible." } };

    QJsonObject wait{ { "type", "integer" },
                      { "minimum", 0 },
                      { "maximum", kMaxWait },
                      { "description", "Seconds that refresh waits for the pages. 0 by default." } };

    return QJsonObject{
        { "name", name() },
        { "description", "Works with a client window of Aspia Client that shows the information about the system "
                         "of a host, its hardware, software, network and operating system. The window must be "
                         "opened before, for example by connect_local_host or connect_router_host with the "
                         "system_info session_type, and its client_id comes from list_clients. The tables are "
                         "the ones that the user sees and saves from the page, in the language of Aspia Client. "
                         "A table with nested rows has the column level first. A table has total_rows, so use "
                         "filter, offset and limit to read a long one by parts. A page of the event log holds "
                         "1000 events. The user sees every page that is read." },
        { "inputSchema", QJsonObject{ { "type", "object" },
                                      { "properties", QJsonObject{ { "client_id", client_id },
                                                                   { "action", action },
                                                                   { "page", page },
                                                                   { "filter", filter },
                                                                   { "offset", offset },
                                                                   { "limit", limit },
                                                                   { "log", log },
                                                                   { "move", move },
                                                                   { "wait", wait } } },
                                      { "required", QJsonArray{ "client_id", "action" } },
                                      { "additionalProperties", false } } } };
}

//--------------------------------------------------------------------------------------------------
void SystemInfoClientTool::call(const QJsonObject& arguments, Reply reply)
{
    std::optional<qint64> client_id = integerArgument(arguments.value("client_id"));
    if (!client_id.has_value())
    {
        reply(false, "The client_id must be an integer.");
        return;
    }

    const QString session_type_error = sessionTypeError(proto::peer::SESSION_TYPE_SYSTEM_INFO);
    if (!session_type_error.isEmpty())
    {
        reply(false, session_type_error);
        return;
    }

    const QString action = arguments.value("action").toString();
    if (action != "pages" && action != "read" && action != "refresh")
    {
        reply(false, "The action must be one of pages, read, refresh.");
        return;
    }

    McpController::SystemInfoWindow* window = McpController::instance().systemInfoWindow(*client_id);
    if (!window)
    {
        if (hasClient(*client_id))
            reply(false, QString("The client %1 is not a system_info client.").arg(*client_id));
        else
            reply(false, QString("There is no client %1. Use list_clients to get the ids.").arg(*client_id));
        return;
    }

    if (action == "pages")
    {
        reply(true, pagesText(*window));
        return;
    }

    if (!isClientConnected(*client_id))
    {
        reply(false, QString("The client %1 is not connected to the host. Its state is in list_clients.")
            .arg(*client_id));
        return;
    }

    if (action == "refresh")
    {
        std::optional<qint64> wait = 0;
        if (arguments.contains("wait"))
            wait = integerArgument(arguments.value("wait"));

        if (!wait.has_value() || *wait < 0 || *wait > kMaxWait)
        {
            reply(false, QString("The wait must be an integer from 0 to %1.").arg(kMaxWait));
            return;
        }

        window->refresh();

        if (*wait == 0)
            reply(true, pagesText(*window));
        else
            waitRefresh(*client_id, *wait, reply);
        return;
    }

    ReadOptions options;
    options.page = arguments.value("page").toString();

    if (!findPage(*window, options.page).has_value())
    {
        reply(false, "The page must be a key from pages.");
        return;
    }

    const QJsonValue filter = arguments.value("filter");
    if (!filter.isUndefined() && !filter.isString())
    {
        reply(false, "The filter must be a string.");
        return;
    }
    options.filter = filter.toString();

    if (arguments.contains("offset"))
    {
        std::optional<qint64> offset = integerArgument(arguments.value("offset"));
        if (!offset.has_value() || *offset < 0)
        {
            reply(false, "The offset must be an integer from 0.");
            return;
        }
        options.offset = *offset;
    }

    if (arguments.contains("limit"))
    {
        std::optional<qint64> limit = integerArgument(arguments.value("limit"));
        if (!limit.has_value() || *limit < 1 || *limit > kMaxLimit)
        {
            reply(false, QString("The limit must be an integer from 1 to %1.").arg(kMaxLimit));
            return;
        }
        options.limit = *limit;
    }

    const bool has_log = arguments.contains("log");
    const bool has_move = arguments.contains("move");

    if ((has_log || has_move) && options.page != kEventLogsPage)
    {
        reply(false, "The log and the move work only with the page event_logs.");
        return;
    }

    if (has_log && has_move)
    {
        reply(false, "Pass the log or the move, the page of a new log is loaded before it can be moved.");
        return;
    }

    std::optional<proto::system_info::EventLogs::Event::Type> log_type;
    if (has_log)
    {
        for (const EventLogName& entry : kEventLogs)
        {
            if (arguments.value("log").toString() == QLatin1String(entry.name))
                log_type = entry.type;
        }

        if (!log_type.has_value())
        {
            reply(false, "The log must be one of application, security, system.");
            return;
        }
    }

    std::optional<SysInfoWidgetEventLogs::Move> move;
    if (has_move)
    {
        for (const MoveName& entry : kMoves)
        {
            if (arguments.value("move").toString() == QLatin1String(entry.name))
                move = entry.move;
        }

        if (!move.has_value())
        {
            reply(false, "The move must be one of newest, newer, older, oldest.");
            return;
        }
    }

    window->showPage(options.page);

    if (log_type.has_value() && !window->selectEventLog(*log_type))
    {
        reply(false, "The window can not choose the log while the page is loading. Read the page again later.");
        return;
    }

    if (move.has_value() && !window->moveEventLog(*move))
    {
        reply(false, QString("The page of the log can not move to %1. It is loading or it is already at the end "
                             "of the log.").arg(arguments.value("move").toString()));
        return;
    }

    waitPage(*client_id, options, reply);
}
