#pragma once

#include <vector>
#include <string>

namespace papi {

    /// Thread/Process logical CPU information
    class SystemInfo {
    public:
        /// @brief Determines the active logical core of the given thread
        /// @param tid id of the thread/process
        /// @return core number or -1 on error
        static int getThreadCpu(int tid = 0);

        /// @brief Lists the active threads of a process
        /// @param pid process id
        /// @return vector of thread ids
        static std::vector<int> listThreads(int pid);
    };

}