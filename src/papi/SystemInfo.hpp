#pragma once

#include <vector>
#include <string>

namespace papi {

//
// Metrics Management
//
struct SystemInfo {
    static int getField(const std::string& fields, int field);

    static int getThreadCpu(int tid = 0);
    static std::vector<int> listThreads(int pid);
};

}