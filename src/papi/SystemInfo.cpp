#include "SystemInfo.hpp"

#include <sched.h>
#include <filesystem>
#include <sstream>
#include <fstream>

#include <iostream>

using namespace papi;

static constexpr auto FieldTaskCpu = 39;
static constexpr auto MaxStatFileSize = 1024; // ~ 255 + (FieldTaskCpu-2) * 22;

// gets the nth field as integer from a /proc/#/stat style string
// the field counting starts at 1!
int SystemInfo::getField(const std::string& fields, int field) {
    int value = -1;
    auto pos = fields.find_last_of(')');
    if ( pos != std::string::npos ) {
        int i;
        for(i=2, pos = pos + 1; 
            i<field-1 && pos != std::string::npos; 
            i++, pos = fields.find_first_of(' ', pos + 1) ) {}
        if ( i == field-1 && pos != std::string::npos ) {
            pos++;
            const auto pos2 = fields.find_first_of(' ', pos);
            if (pos2 != std::string::npos) {
                auto field = fields.substr(pos, pos2-pos);
                std::stringstream sstream{field};
                sstream >> value;
            }
        }
    }
    return value;
}

int SystemInfo::getThreadCpu(int tid) {
    if ( tid == 0 ) return sched_getcpu();
    
    int cpu = -1;
    std::string path = "/proc/" + std::to_string(tid) + "/stat";
    std::ifstream statFile{path, std::ios::in | std::ios::binary};
    if ( statFile.is_open() ) {
        std::string stat(MaxStatFileSize, '\0');
        statFile.read(stat.data(), MaxStatFileSize);
        stat.resize(statFile.gcount());
        cpu = getField(stat, FieldTaskCpu);
        statFile.close();
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
