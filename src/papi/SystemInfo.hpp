#pragma once

#include <vector>

namespace papi {

//
// Metrics Management
//
struct SystemInfo {
    static int getThreadCpu(int tid = 0);
    static std::vector<int> listThreads(int pid);
};

}