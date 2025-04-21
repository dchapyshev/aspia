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

#ifndef HOST_PROCESS_MONITOR_H
#define HOST_PROCESS_MONITOR_H

#include "base/macros_magic.h"

#include <cstdint>
#include <string>
#include <map>
#include <vector>

namespace host {

class ProcessMonitor
{
public:
    ProcessMonitor();
    ~ProcessMonitor();

    struct ProcessEntry
    {
        bool process_name_changed = false;
        std::string process_name;

        bool user_name_changed = false;
        std::string user_name;

        bool file_path_changed = false;
        std::string file_path;

        int64_t cpu_time = 0;
        int32_t cpu_ratio = 0;
        uint32_t session_id = 0;
        int64_t mem_private_working_set = 0;
        int64_t mem_working_set = 0;
        int64_t mem_peak_working_set = 0;
        int64_t mem_working_set_delta = 0;
        uint32_t thread_count = 0;
    };

    using ProcessId = uint32_t;
    using ProcessMap = std::map<ProcessId, ProcessEntry>;

    const ProcessMap& processes(bool reset_cache);
    int calcCpuUsage();
    int calcMemoryUsage();

    bool endProcess(ProcessId process_id);

private:
    bool updateSnapshot();
    void updateTable();

    void* ntdll_library_ = nullptr;
    void* nt_query_system_info_func_ = nullptr;

    std::vector<uint8_t> snapshot_;
    ProcessMap table_;

    static const uint32_t kMaxCpuCount = 64;

    uint32_t processor_count_ = 0;
    int64_t prev_cpu_idle_time_[kMaxCpuCount];
    int64_t prev_cpu_total_time_[kMaxCpuCount];

    DISALLOW_COPY_AND_ASSIGN(ProcessMonitor);
};

} // namespace host

#endif // HOST_PROCESS_MONITOR_H
