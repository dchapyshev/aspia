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

#ifndef HOST_PROCESS_MONITOR_H
#define HOST_PROCESS_MONITOR_H

#include <QMap>
#include <QString>

#include <memory>

class ProcessMonitor
{
public:
    virtual ~ProcessMonitor() = default;

    struct ProcessEntry
    {
        bool process_name_changed = false;
        QString process_name;

        bool user_name_changed = false;
        QString user_name;

        bool file_path_changed = false;
        QString file_path;

        qint64 cpu_time = 0;
        qint32 cpu_ratio = 0;
        quint32 session_id = 0;
        qint64 mem_private_working_set = 0;
        qint64 mem_working_set = 0;
        qint64 mem_peak_working_set = 0;
        qint64 mem_working_set_delta = 0;
        quint32 thread_count = 0;
    };

    using ProcessId = quint32;
    using ProcessMap = QMap<ProcessId, ProcessEntry>;

    static std::unique_ptr<ProcessMonitor> create();

    virtual const ProcessMap& processes(bool reset_cache) = 0;
    virtual int calcCpuUsage() = 0;
    virtual int calcMemoryUsage() = 0;
    virtual bool endProcess(ProcessId process_id) = 0;

protected:
    ProcessMonitor() = default;

private:
    Q_DISABLE_COPY_MOVE(ProcessMonitor)
};

#endif // HOST_PROCESS_MONITOR_H
