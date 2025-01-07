#include "process_monitor.h"

int64_t getTotalPhysicalRAM() {
    uint64_t total_RAM;
    size_t len = sizeof(total_RAM);
    if (sysctlbyname("hw.memsize", &total_RAM, &len, NULL, 0) == 0) {
        return total_RAM;
    } 
    return 16 * 1e9;
}

// Use mach library to get Total CPU Usage Information
double getCPUUsage() {
    natural_t cpuCount;
    processor_info_array_t cpuInfo;
    mach_msg_type_number_t cpuInfoCount;

    host_processor_info(mach_host_self(), PROCESSOR_CPU_LOAD_INFO, &cpuCount, &cpuInfo, &cpuInfoCount);

    static std::vector<unsigned long> prevUser (cpuCount, 0);
    static std::vector<unsigned long> prevSystem (cpuCount, 0);
    static std::vector<unsigned long> prevIdle (cpuCount, 0);

    unsigned long totalUser = 0, totalSystem = 0, totalIdle = 0;
    for (natural_t i = 0; i < cpuCount; i++) {
        natural_t offset = i * CPU_STATE_MAX;
        totalUser += cpuInfo[offset + CPU_STATE_USER] - prevUser[i];
        totalSystem += cpuInfo[offset + CPU_STATE_SYSTEM] - prevSystem[i];
        totalIdle += cpuInfo[offset + CPU_STATE_IDLE] - prevIdle[i];

        prevUser[i] = cpuInfo[offset + CPU_STATE_USER];
        prevSystem[i] = cpuInfo[offset + CPU_STATE_SYSTEM];
        prevIdle[i] = cpuInfo[offset + CPU_STATE_IDLE];
    }

    vm_deallocate(mach_task_self(), (vm_address_t)cpuInfo, cpuInfoCount * sizeof(integer_t));

    unsigned long total = totalUser + totalSystem + totalIdle;
    return (total > 0 ? (static_cast<double>(totalUser + totalSystem) / total) * 100.0 : 0.0);
}

SystemSummary getSystemSummary() {
    SystemSummary syssum = {0.0, 0.0, 0, 0};

    syssum.totalRAM = getTotalPhysicalRAM();
    syssum.totalCPUUsage = getCPUUsage();

    vm_statistics64_data_t vmStats;
    mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
    host_statistics64(mach_host_self(), HOST_VM_INFO64, (host_info64_t)&vmStats, &count);

    // Inactive and speculative is basically free and should be considered as such
    uint64_t freeRAM = static_cast<uint64_t>(vmStats.free_count + vmStats.inactive_count + vmStats.speculative_count) * sysconf(_SC_PAGESIZE);
    uint64_t usedRAM = syssum.totalRAM - freeRAM;

    // Account for compressed memory used by the system that was included in the above freeRAM calculation
    uint64_t compressedMemory = static_cast<uint64_t>(vmStats.compressor_page_count) * sysconf(_SC_PAGESIZE);
    usedRAM += compressedMemory;
    
    // This value still differs from the mac activity monitor by up to 1GB
    // TODO: Look into why this is the case
    syssum.totalUsedRAM = usedRAM;
    syssum.totalRAMUsage = (syssum.totalRAM > 0) ? (static_cast<double>(usedRAM) / syssum.totalRAM) * 100.0 : 0.0;

    return syssum;
}

void getProcessDetails(vector<ProcessInfo>& procInfo, SystemSummary& syssum, map<pid_t, ProcessStats>& prevStats) {

    // Get PIDs
    pid_t pids [2048];
    int bytesUsed = proc_listpids(PROC_ALL_PIDS, 0, pids, sizeof(pids));
    int count = bytesUsed / sizeof(pid_t);

    // For each PID, collect info
    for (int i = 0; i < count; i++) {
        pid_t pid = pids[i];

        // Kernel process with pid 0 is invisible to user space
        if (pid == 0) continue; 

        struct proc_taskallinfo info;
        int ret = proc_pidinfo(pid, PROC_PIDTASKALLINFO, 0, &info, sizeof(info));
        if (ret <= 0) continue; // Unable to get information

        // Get process name
        char nameBuffer[PROC_PIDPATHINFO_MAXSIZE];
        if (proc_name(pid, nameBuffer, sizeof(nameBuffer)) == 0) {
            strcpy(nameBuffer, "Unknown");
        }

        std::string name (nameBuffer);
        if (name.length() > MAX_NAME_LENGTH) {
            name = name.substr(0, MAX_NAME_LENGTH - 3) + "...";
        }

        // Memory usage (Physical RAM) - Resident Set Size
        uint64_t rss = info.ptinfo.pti_resident_size;
        double rssMB = (double)rss / (1024 * 1024);
        double ramPercent = ((double)rss / syssum.totalRAM) * 100.0;

        // CPU Time (nanoseconds)
        uint64_t userTime = info.ptinfo.pti_total_user;
        uint64_t systemTime = info.ptinfo.pti_total_system;

        // Calculate CPU usage delta if this PID has been previously seen
        double cpuPercent = 0.0;
        if (prevStats.find(pid) != prevStats.end()) {
            uint64_t deltaUser = userTime - prevStats[pid].lastUserTime;
            uint64_t deltaSystem = systemTime - prevStats[pid].lastSystemTime;
            uint64_t totalDelta = deltaUser + deltaSystem;

            double elapsedSeconds = 1.0;
            double cpuSeconds = (double)totalDelta / (1024 * 1024 * 1024); // Convert to seconds
            cpuPercent = (cpuSeconds / elapsedSeconds) * 100.0;
        }

        ProcessStats stats = { userTime, systemTime };
        prevStats[pid] = stats;

        // Store process info in vector
        procInfo.emplace_back(pid, name, cpuPercent, rssMB, ramPercent);
    }
}

void sortAndGroupProcesses(vector<ProcessInfo>& procInfo, int state, bool is_group) {
    if (is_group) {
        // Group processes by their name 
        // TODO: -- 1 more option - Parse names until first ( and remove the rest
        unordered_map<string, ProcessInfo> mp;
        for (ProcessInfo p : procInfo) {
            if (mp.find(p.name) == mp.end()) {
                mp.emplace(p.name, ProcessInfo(p.pid, p.name, p.cpuPercent, p.rssMB, p.ramPercent));
            } else {
                // Add metrics to map for this
                mp[p.name].cpuPercent += p.cpuPercent;
                mp[p.name].rssMB += p.rssMB;
                mp[p.name].ramPercent += p.ramPercent;
            }
        }

        procInfo.clear();

        for (auto m : mp) {
            procInfo.emplace_back(m.second);
        }
    }

    if (state == VIEW) {
        // No sorting, do nothing
    } else if (state == SORT_BY_CPU) {
        sort(procInfo.begin(), procInfo.end(), [](ProcessInfo& a, ProcessInfo& b) {
            return a.cpuPercent > b.cpuPercent;
        });
    } else if (state == SORT_BY_RAM) {
        sort(procInfo.begin(), procInfo.end(), [](ProcessInfo& a, ProcessInfo& b) {
            return a.rssMB > b.rssMB;
        });
    }
}



