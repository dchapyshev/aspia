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

#include "host/process_monitor_linux.h"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QSet>

#include <pwd.h>
#include <signal.h>
#include <unistd.h>

#include <utility>

#include "base/logging.h"

namespace {

struct ProcessRaw
{
    ProcessMonitor::ProcessId pid = 0;
    QString name;
    uint uid = 0;
    qint64 cpu_time = 0;
    quint32 session_id = 0;
    qint64 mem_working_set = 0;
    qint64 mem_private_working_set = 0;
    qint64 mem_peak_working_set = 0;
    quint32 thread_count = 0;
};

//--------------------------------------------------------------------------------------------------
qint32 calcCpuRatio(qint64 cpu_time_delta, qint64 total_time_delta)
{
    if (total_time_delta <= 0)
        return 0;

    qint64 cpu_ratio = (cpu_time_delta * 100LL + (total_time_delta / 2LL)) / total_time_delta;
    if (cpu_ratio > 99)
        cpu_ratio = 99;
    if (cpu_ratio < 0)
        cpu_ratio = 0;

    return static_cast<qint32>(cpu_ratio);
}

//--------------------------------------------------------------------------------------------------
QByteArray readProcFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return QByteArray();

    // Files under /proc report a size of zero, so read line by line until exhausted.
    QByteArray content;
    for (;;)
    {
        QByteArray line = file.readLine();
        if (line.isEmpty())
            break;
        content += line;
    }

    return content;
}

//--------------------------------------------------------------------------------------------------
QString userNameByUid(uint uid)
{
    passwd* pw = getpwuid(static_cast<uid_t>(uid));
    if (!pw || !pw->pw_name)
        return QString::number(uid);

    return QString::fromLocal8Bit(pw->pw_name);
}

//--------------------------------------------------------------------------------------------------
QString readExePath(ProcessMonitor::ProcessId pid)
{
    return QFile::symLinkTarget("/proc/" + QString::number(pid) + "/exe");
}

//--------------------------------------------------------------------------------------------------
bool readProcessStat(ProcessMonitor::ProcessId pid, ProcessRaw* raw)
{
    const QByteArray stat = readProcFile("/proc/" + QString::number(pid) + "/stat");
    if (stat.isEmpty())
        return false;

    // The comm field (2) is wrapped in parentheses and may contain spaces or ')'; everything after
    // the last ')' is single-space separated and starts at field 3 (state).
    int rparen = stat.lastIndexOf(')');
    if (rparen < 0)
        return false;

    const QList<QByteArray> fields = stat.mid(rparen + 2).trimmed().split(' ');
    if (fields.size() < 13)
        return false;

    // Skip zombie (defunct) processes: they have already exited and are awaiting reaping by their
    // parent, so they have no executable image or memory.
    if (fields[0] == "Z")
        return false;

    // fields[k] is stat field (3 + k): session = field 6, utime = field 14, stime = field 15.
    raw->session_id = fields[3].toUInt();
    raw->cpu_time = fields[11].toLongLong() + fields[12].toLongLong();
    return true;
}

//--------------------------------------------------------------------------------------------------
bool readProcessStatus(ProcessMonitor::ProcessId pid, ProcessRaw* raw)
{
    const QByteArray status = readProcFile("/proc/" + QString::number(pid) + "/status");
    if (status.isEmpty())
        return false;

    const QList<QByteArray> lines = status.split('\n');
    for (const QByteArray& line : std::as_const(lines))
    {
        int colon = line.indexOf(':');
        if (colon < 0)
            continue;

        const QByteArray key = line.left(colon);
        const QByteArray value = line.mid(colon + 1).simplified();

        if (key == "Name")
            raw->name = QString::fromLocal8Bit(value);
        else if (key == "Uid")
            raw->uid = value.split(' ').first().toUInt();
        else if (key == "VmRSS")
            raw->mem_working_set = value.split(' ').first().toLongLong() * 1024LL;
        else if (key == "RssAnon")
            raw->mem_private_working_set = value.split(' ').first().toLongLong() * 1024LL;
        else if (key == "VmHWM")
            raw->mem_peak_working_set = value.split(' ').first().toLongLong() * 1024LL;
        else if (key == "Threads")
            raw->thread_count = value.toUInt();
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
QList<ProcessRaw> readSnapshot()
{
    QList<ProcessRaw> result;

    const QStringList entries = QDir("/proc").entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString& entry : std::as_const(entries))
    {
        bool ok = false;
        ProcessMonitor::ProcessId pid = entry.toUInt(&ok);
        if (!ok)
            continue;

        ProcessRaw raw;
        raw.pid = pid;

        if (!readProcessStat(pid, &raw))
            continue;
        if (!readProcessStatus(pid, &raw))
            continue;

        result.append(raw);
    }

    return result;
}

} // namespace

//--------------------------------------------------------------------------------------------------
ProcessMonitorLinux::ProcessMonitorLinux()
{
    LOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
ProcessMonitorLinux::~ProcessMonitorLinux()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
const ProcessMonitor::ProcessMap& ProcessMonitorLinux::processes(bool reset_cache)
{
    if (reset_cache)
        table_.clear();

    const QList<ProcessRaw> snapshot = readSnapshot();

    // Total CPU time consumed by all processes since the previous snapshot.
    qint64 total_time_delta = 0;
    for (const ProcessRaw& raw : std::as_const(snapshot))
    {
        auto it = table_.constFind(raw.pid);
        qint64 prev_cpu_time = (it != table_.constEnd()) ? it.value().cpu_time : 0;
        qint64 delta = raw.cpu_time - prev_cpu_time;
        if (delta > 0)
            total_time_delta += delta;
    }

    QSet<ProcessId> active_pids;

    for (const ProcessRaw& raw : std::as_const(snapshot))
    {
        active_pids.insert(raw.pid);

        auto it = table_.find(raw.pid);
        bool is_new = (it == table_.end());
        if (is_new)
            it = table_.insert(raw.pid, ProcessEntry());

        ProcessEntry& entry = it.value();

        qint64 cpu_time_delta = raw.cpu_time - entry.cpu_time;

        entry.cpu_ratio               = calcCpuRatio(cpu_time_delta, total_time_delta);
        entry.session_id              = raw.session_id;
        entry.mem_working_set_delta   = raw.mem_working_set - entry.mem_working_set;
        entry.mem_private_working_set = raw.mem_private_working_set;
        entry.mem_working_set         = raw.mem_working_set;
        entry.mem_peak_working_set    = raw.mem_peak_working_set;
        entry.thread_count            = raw.thread_count;
        entry.cpu_time                = raw.cpu_time;

        entry.process_name_changed = false;
        entry.user_name_changed = false;
        entry.file_path_changed = false;

        if (is_new)
        {
            QString file_path = readExePath(raw.pid);

            // The comm field (raw.name) is truncated to 15 characters; prefer the executable name and
            // fall back to comm for processes without an accessible executable (e.g. kernel threads).
            entry.process_name = file_path.isEmpty() ? raw.name : file_path.section(QChar('/'), -1);
            entry.process_name_changed = true;

            entry.user_name = userNameByUid(raw.uid);
            entry.user_name_changed = true;

            entry.file_path = file_path;
            entry.file_path_changed = true;
        }
    }

    // Remove obsolete processes.
    for (auto it = table_.begin(); it != table_.end();)
    {
        if (!active_pids.contains(it.key()))
            it = table_.erase(it);
        else
            ++it;
    }

    return table_;
}

//--------------------------------------------------------------------------------------------------
int ProcessMonitorLinux::calcCpuUsage()
{
    const QByteArray content = readProcFile("/proc/stat");

    qint64 idle_time = 0;
    qint64 total_time = 0;

    const QList<QByteArray> lines = content.split('\n');
    for (const QByteArray& line : std::as_const(lines))
    {
        if (!line.startsWith("cpu "))
            continue;

        // cpu user nice system idle iowait irq softirq steal guest guest_nice
        const QList<QByteArray> fields = line.simplified().split(' ');
        for (int i = 1; i < fields.size(); ++i)
            total_time += fields[i].toLongLong();

        if (fields.size() > 5)
            idle_time = fields[4].toLongLong() + fields[5].toLongLong();
        break;
    }

    qint64 idle_delta = idle_time - prev_cpu_idle_time_;
    qint64 total_delta = total_time - prev_cpu_total_time_;

    prev_cpu_idle_time_ = idle_time;
    prev_cpu_total_time_ = total_time;

    if (total_delta <= 0)
        return 0;

    return static_cast<int>((100LL * (total_delta - idle_delta)) / total_delta);
}

//--------------------------------------------------------------------------------------------------
int ProcessMonitorLinux::calcMemoryUsage()
{
    const QByteArray content = readProcFile("/proc/meminfo");

    qint64 mem_total = 0;
    qint64 mem_available = 0;

    const QList<QByteArray> lines = content.split('\n');
    for (const QByteArray& line : std::as_const(lines))
    {
        if (line.startsWith("MemTotal:"))
            mem_total = line.mid(line.indexOf(':') + 1).simplified().split(' ').first().toLongLong();
        else if (line.startsWith("MemAvailable:"))
            mem_available = line.mid(line.indexOf(':') + 1).simplified().split(' ').first().toLongLong();
    }

    if (mem_total <= 0)
        return 0;

    return static_cast<int>((100LL * (mem_total - mem_available)) / mem_total);
}

//--------------------------------------------------------------------------------------------------
bool ProcessMonitorLinux::endProcess(ProcessId process_id)
{
    if (!table_.contains(process_id))
    {
        LOG(ERROR) << "Process not found in table";
        return false;
    }

    if (process_id <= 1)
    {
        LOG(ERROR) << "Unable to end system process";
        return false;
    }

    if (kill(static_cast<pid_t>(process_id), SIGKILL) != 0)
    {
        PLOG(ERROR) << "kill failed";
        return false;
    }

    return true;
}
