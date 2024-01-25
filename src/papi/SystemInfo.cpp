#include "SystemInfo.hpp"

#include <sched.h>
#include <string>
#include <filesystem>
#include <sstream>

using namespace papi;

static inline int get_cpu(const cpu_set_t& mask) {
    for(int i=0; i<CPU_SETSIZE; ++i) {
        if ( CPU_ISSET(i, &mask) ) return i;
    }
    return -1;
}

int SystemInfo::getThreadCpu(int tid)
{
    if ( tid == 0 ) return sched_getcpu();

    cpu_set_t mask;
    CPU_ZERO(&mask);
    int rv = sched_getaffinity(tid, sizeof(mask), &mask);
    if ( rv != 0 ) {
        return -1;
    }
    return get_cpu(mask);
}

std::vector<int> papi::SystemInfo::listThreads(int pid) {
    std::string path = "/proc/" + std::to_string(pid) + "/task";
    std::vector<int> tids;
    for(const auto& entry: std::filesystem::directory_iterator(path)) {
        std::stringstream s{entry.path().filename()};
        int tid;
        s >> tid;
        tids.push_back(tid);
    }
    return tids;
}
