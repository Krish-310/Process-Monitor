#include <iostream>
#include <unistd.h>
#include <chrono>
#include <map>
#include <vector>
#include <ncurses.h>

using namespace std;

#include "global_definitions.h"
#include "process_monitor.cpp"

void displayProcessInfo(vector<ProcessInfo>& procInfo, SystemSummary syssum) {

    printw("CPU Usage: %6.2f%% | RAM Usage: %6.2f%% (%llu / %llu MB)\n",
            syssum.totalCPUUsage,
            syssum.totalRAMUsage,
            syssum.totalUsedRAM / (1024 * 1024),
            syssum.totalRAM / (1024 * 1024));

    printw("+--------+-----------------------------+---------+------------+---------+\n");
    printw("| PID    | Name                        | CPU%%    | Memory(MB) | RAM%%    |\n");
    printw("+--------+-----------------------------+---------+------------+---------+\n");

    for (ProcessInfo p : procInfo) {
        printw("| %-6d | %-27s | %6.2f%% | %10.4f | %6.3f%% |\n", 
                p.pid, p.name.c_str(), p.cpuPercent, p.rssMB, p.ramPercent);
    }

    move(LINES - 2, 0);
    printw("+--------+-----------------------------+---------+------------+---------+\n");
    move(LINES - 1, 0);
    clrtoeol();
    mvprintw(LINES - 1, 0, "Press 'q' to quit and 'h' for help...");
}

void displayHelpPage() {
    printw("+------------------- HELP -------------------+\n");
    printw("| h: Show this help page                     |\n");
    printw("| v: Display processes unsorted              |\n");
    printw("| r: Sort processes by RAM percentage        |\n");
    printw("| c: Sort processes by CPU percentage        |\n");
    printw("| g: Turn On/Off Group Mode (by Name)        |\n");
    printw("| q: Quit                                    |\n");
    printw("+--------------------------------------------+\n");
    move(LINES - 1, 0);
    clrtoeol();
    mvprintw(LINES - 1, 0, "Press 'q' to quit...");
}

void runProcessMonitor() {
    initscr(); // Start ncurses
    noecho(); // Do not echo typed chars
    cbreak(); // Disable line buffering 
    keypad(stdscr, TRUE); // Enable special keys

    AppState state = VIEW;
    bool is_group = false;

    SystemSummary syssum = getSystemSummary();
    map<pid_t, ProcessStats> prevStats;
    vector<ProcessInfo> procInfo;

    timeout(1000); // getch() modified to only block for up to 1s

    while (true) {
        clear();

        if (state == VIEW_HELP) {
            displayHelpPage();
        } else {
            getProcessDetails(procInfo, syssum, prevStats);

            // Get system summary again
            syssum = getSystemSummary();

            sortAndGroupProcesses(procInfo, state, is_group);

            // Sort and print process information 
            displayProcessInfo(procInfo, syssum);
        }

        refresh();

        int ch = getch();
        switch (ch) {
            case 'h':
                state = VIEW_HELP;
                break;
            case 'r':
                state = SORT_BY_RAM;
                break;
            case 'c':
                state = SORT_BY_CPU;
                break;
            case 'v':
                state = VIEW;
                break;
            case 'g':
                is_group = !is_group;
                break;
            default:
                break;
        }
        if (ch == 'q') break;

        procInfo.clear(); // Clear vector
    }

    endwin();
}


int main() {
    runProcessMonitor();
}
