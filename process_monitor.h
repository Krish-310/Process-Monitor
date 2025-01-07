#ifndef PROCESS_MONITOR_H
#define PROCESS_MONITOR_H

#include <iostream>
#include <unistd.h>
#include <chrono>
#include <map>
#include <vector>
#include <libproc.h> 
#include <sys/sysctl.h>
#include <mach/mach.h>
#include <mach/processor_info.h>
#include <mach/mach_host.h>

#include "global_definitions.h"

using namespace std;

struct SystemSummary {
    double totalCPUUsage;
    double totalRAMUsage;
    uint64_t totalUsedRAM;
    uint64_t totalRAM;
};

struct ProcessStats {
    uint64_t lastUserTime;
    uint64_t lastSystemTime;
};

struct ProcessInfo {
    pid_t pid;
    string name;
    double cpuPercent;
    double rssMB;
    double ramPercent;

    ProcessInfo(): pid(0), name(""), cpuPercent(0.0), rssMB(0.0), ramPercent(0.0) {}

    ProcessInfo(int p, const std::string& n, double cpu, double rss, double ram): 
                pid(p), name(n), cpuPercent(cpu), rssMB(rss), ramPercent(ram) {}
};

int64_t getTotalPhysicalRAM();

double getCPUUsage();

SystemSummary getSystemSummary();

void getProcessDetails(vector<ProcessInfo>& procInfo, SystemSummary& syssum, map<pid_t, ProcessStats>& prevStats);

void sortAndGroupProcesses(vector<ProcessInfo>& procInfo, int state, bool is_group);

#endif
