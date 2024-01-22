#include "SystemInfo.hpp"

#include <sched.h>

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