#include "SystemInfo.hpp"

#include <sched.h>
#include <string>
#include <filesystem>
#include <sstream>
#include <fstream>

using namespace papi;

int SystemInfo::getThreadCpu(int tid) {
    int cpu = -1;
    std::string path = "/proc/" + std::to_string(tid) + "/stat";
    std::ifstream statFile{path};
    if ( statFile.is_open() ) {
        std::string field;
        for (int i = 0; i < 39; ++i) {
            statFile >> field;
        }
        statFile.close();

        std::stringstream ss{field};
        ss >> cpu;
    }
    return cpu;
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