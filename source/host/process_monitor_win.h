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

#ifndef HOST_PROCESS_MONITOR_WIN_H
#define HOST_PROCESS_MONITOR_WIN_H

#include "host/process_monitor.h"

#include <QByteArray>

class ProcessMonitorWin final : public ProcessMonitor
{
public:
    ProcessMonitorWin();
    ~ProcessMonitorWin() final;

    // ProcessMonitor implementation.
    const ProcessMap& processes(bool reset_cache) final;
    int calcCpuUsage() final;
    int calcMemoryUsage() final;
    bool endProcess(ProcessId process_id) final;

private:
    bool updateSnapshot();
    void updateTable();

    void* ntdll_library_ = nullptr;
    void* nt_query_system_info_func_ = nullptr;

    QByteArray snapshot_;
    ProcessMap table_;

    static const quint32 kMaxCpuCount = 64;

    quint32 processor_count_ = 0;
    qint64 prev_cpu_idle_time_[kMaxCpuCount];
    qint64 prev_cpu_total_time_[kMaxCpuCount];

    Q_DISABLE_COPY_MOVE(ProcessMonitorWin)
};

#endif // HOST_PROCESS_MONITOR_WIN_H
