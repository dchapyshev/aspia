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

#include "host/process_monitor_mac.h"

#include <QSet>

#include <algorithm>
#include <vector>

#include <libproc.h>
#include <mach/mach.h>
#include <pwd.h>
#include <signal.h>
#include <sys/proc_info.h>
#include <sys/sysctl.h>

#include "base/logging.h"

namespace {

//--------------------------------------------------------------------------------------------------
struct ProcessRaw
{
    ProcessMonitor::ProcessId pid = 0;
    QString name;
    uint uid = 0;
    qint64 cpu_time = 0;                 // user + system, in nanoseconds
    qint64 mem_working_set = 0;          // resident size
    qint64 mem_private_working_set = 0;
    quint32 thread_count = 0;
    qint64 start_time = 0;               // microseconds since epoch, identity token for PID reuse
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
QString userNameByUid(uint uid)
{
    passwd* pw = getpwuid(static_cast<uid_t>(uid));
    if (!pw || !pw->pw_name)
        return QString::number(uid);

    return QString::fromUtf8(pw->pw_name);
}

//--------------------------------------------------------------------------------------------------
QString readExePath(ProcessMonitor::ProcessId pid)
{
    char buffer[PROC_PIDPATHINFO_MAXSIZE];
    if (proc_pidpath(static_cast<int>(pid), buffer, sizeof(buffer)) > 0)
        return QString::fromUtf8(buffer);

    return QString();
}

//--------------------------------------------------------------------------------------------------
// Reads task + BSD info for a single process. Returns false when the process is gone or inaccessible
// (proc_pidinfo needs root or the same uid; the service runs as root, so it sees every process).
bool readProcessInfo(ProcessMonitor::ProcessId pid, ProcessRaw* raw)
{
    struct proc_taskallinfo info;
    const int size = proc_pidinfo(static_cast<int>(pid), PROC_PIDTASKALLINFO, 0, &info, sizeof(info));
    if (size < static_cast<int>(sizeof(info)))
        return false;

    raw->pid = pid;
    raw->uid = info.pbsd.pbi_uid;
    raw->cpu_time = static_cast<qint64>(info.ptinfo.pti_total_user + info.ptinfo.pti_total_system);
    raw->mem_working_set = static_cast<qint64>(info.ptinfo.pti_resident_size);

    // macOS has no cheap per-process private-vs-shared split; use the resident size for both.
    raw->mem_private_working_set = raw->mem_working_set;
    raw->thread_count = info.ptinfo.pti_threadnum;
    raw->start_time = static_cast<qint64>(info.pbsd.pbi_start_tvsec) * 1000000LL +
                      static_cast<qint64>(info.pbsd.pbi_start_tvusec);

    // pbi_name is the fuller accounting name; pbi_comm is truncated to 16 chars.
    raw->name = QString::fromUtf8(info.pbsd.pbi_name[0] ? info.pbsd.pbi_name : info.pbsd.pbi_comm);
    return true;
}

//--------------------------------------------------------------------------------------------------
QList<ProcessRaw> readSnapshot()
{
    QList<ProcessRaw> result;

    int bytes = proc_listpids(PROC_ALL_PIDS, 0, nullptr, 0);
    if (bytes <= 0)
        return result;

    std::vector<pid_t> pids(bytes / sizeof(pid_t));
    bytes = proc_listpids(PROC_ALL_PIDS, 0, pids.data(), static_cast<int>(pids.size() * sizeof(pid_t)));
    if (bytes <= 0)
        return result;

    const int count = bytes / static_cast<int>(sizeof(pid_t));
    for (int i = 0; i < count; ++i)
    {
        if (pids[i] <= 0)
            continue;

        ProcessRaw raw;
        if (readProcessInfo(static_cast<ProcessMonitor::ProcessId>(pids[i]), &raw))
            result.append(raw);
    }

    return result;
}

} // namespace

//--------------------------------------------------------------------------------------------------
ProcessMonitorMac::ProcessMonitorMac()
{
    LOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
ProcessMonitorMac::~ProcessMonitorMac()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
const ProcessMonitor::ProcessMap& ProcessMonitorMac::processes(bool reset_cache)
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
        entry.session_id              = 0; // No Windows-style session on macOS.
        entry.mem_working_set_delta   = raw.mem_working_set - entry.mem_working_set;
        entry.mem_private_working_set = raw.mem_private_working_set;
        entry.mem_working_set         = raw.mem_working_set;
        // proc_pidinfo does not expose a peak resident size, so track the maximum seen ourselves.
        entry.mem_peak_working_set    = std::max(entry.mem_peak_working_set, raw.mem_working_set);
        entry.thread_count            = raw.thread_count;
        entry.cpu_time                = raw.cpu_time;
        entry.start_time              = raw.start_time;

        entry.process_name_changed = false;
        entry.user_name_changed = false;
        entry.file_path_changed = false;

        if (is_new)
        {
            QString file_path = readExePath(raw.pid);

            // Prefer the executable name; fall back to the accounting name for processes without an
            // accessible executable (kernel_task and similar).
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
int ProcessMonitorMac::calcCpuUsage()
{
    host_cpu_load_info_data_t info;
    mach_msg_type_number_t count = HOST_CPU_LOAD_INFO_COUNT;
    if (host_statistics(mach_host_self(), HOST_CPU_LOAD_INFO,
                        reinterpret_cast<host_info_t>(&info), &count) != KERN_SUCCESS)
    {
        return 0;
    }

    const quint64 idle = info.cpu_ticks[CPU_STATE_IDLE];
    const quint64 total = info.cpu_ticks[CPU_STATE_USER] + info.cpu_ticks[CPU_STATE_SYSTEM] +
                          info.cpu_ticks[CPU_STATE_NICE] + idle;

    const quint64 idle_delta = idle - prev_cpu_idle_;
    const quint64 total_delta = total - prev_cpu_total_;

    prev_cpu_idle_ = idle;
    prev_cpu_total_ = total;

    if (total_delta == 0)
        return 0;

    return static_cast<int>((100ULL * (total_delta - idle_delta)) / total_delta);
}

//--------------------------------------------------------------------------------------------------
int ProcessMonitorMac::calcMemoryUsage()
{
    int64_t total_memory = 0;
    size_t length = sizeof(total_memory);
    if (sysctlbyname("hw.memsize", &total_memory, &length, nullptr, 0) != 0 || total_memory <= 0)
        return 0;

    vm_statistics64_data_t vm_stat;
    mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
    if (host_statistics64(mach_host_self(), HOST_VM_INFO64,
                          reinterpret_cast<host_info64_t>(&vm_stat), &count) != KERN_SUCCESS)
    {
        return 0;
    }

    vm_size_t page_size = 0;
    if (host_page_size(mach_host_self(), &page_size) != KERN_SUCCESS)
        return 0;

    // "In use" the way Activity Monitor reports it: active + wired + compressed pages.
    const quint64 used_pages = static_cast<quint64>(vm_stat.active_count) +
                               vm_stat.wire_count + vm_stat.compressor_page_count;
    const quint64 used = used_pages * static_cast<quint64>(page_size);

    return static_cast<int>((100ULL * used) / static_cast<quint64>(total_memory));
}

//--------------------------------------------------------------------------------------------------
bool ProcessMonitorMac::endProcess(ProcessId process_id)
{
    const auto it = table_.constFind(process_id);
    if (it == table_.constEnd())
    {
        LOG(ERROR) << "Process not found in table";
        return false;
    }

    if (process_id <= 1)
    {
        LOG(ERROR) << "Unable to end system process";
        return false;
    }

    // Guard against PID reuse: the pid comes from a snapshot the client saw earlier, and the process
    // may have exited with the pid recycled since. Re-read the live start time and refuse to kill if
    // it no longer matches the snapshot's identity token.
    ProcessRaw live;
    if (!readProcessInfo(process_id, &live) || live.start_time != it.value().start_time)
    {
        LOG(ERROR) << "Process identity mismatch (possible PID reuse); not terminating";
        return false;
    }

    if (kill(static_cast<pid_t>(process_id), SIGKILL) != 0)
    {
        PLOG(ERROR) << "kill failed";
        return false;
    }

    return true;
}
