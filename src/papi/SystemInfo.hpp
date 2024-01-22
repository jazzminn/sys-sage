#pragma once

namespace papi {

//
// Metrics Management
//
struct SystemInfo {
    static int getThreadCpu(int tid = 0);
};

}