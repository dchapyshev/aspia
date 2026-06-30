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

#ifndef HOST_PROCESS_MONITOR_MAC_H
#define HOST_PROCESS_MONITOR_MAC_H

#include "host/process_monitor.h"

class ProcessMonitorMac final : public ProcessMonitor
{
public:
    ProcessMonitorMac();
    ~ProcessMonitorMac() final;

    // ProcessMonitor implementation.
    const ProcessMap& processes(bool reset_cache) final;
    int calcCpuUsage() final;
    int calcMemoryUsage() final;
    bool endProcess(ProcessId process_id) final;

private:
    ProcessMap table_;

    Q_DISABLE_COPY_MOVE(ProcessMonitorMac)
};

#endif // HOST_PROCESS_MONITOR_MAC_H
