/*
 * green_score.cpp
 * ---------------
 * Emitting Green Score of a target program, based on the original green_score.cpp by Stepan Vanecek
 * It uses the new Measurement API of sys-sage.
 * 
 */

#include <chrono>
#include <vector>
#include <string>
#include <iostream>
#include <iomanip>

#include <unistd.h>
#include <sys/ptrace.h>
#include <sys/wait.h>

#include "sys-sage.hpp"

static const std::string greenScoreRegion{"green_score"};
static std::vector<std::string> papi_events = {
    "PAPI_TOT_INS",
    "PAPI_L2_TCA",
    "PAPI_L3_LDM",
    "PAPI_L3_TCM"
};

struct GreenScore {
    struct Entry {
        uint64_t time;
        int hwThread;
        double frequency;
        std::vector<long long> papi_counters;

        void Print()
        {
            std::cout 
                << std::setw(12) << time 
                << std::setw(12) << hwThread
                << std::setw(12) << frequency;
            for(auto& c: papi_counters) {
                std::cout << std::setw(12) << c;
            }
            std::cout << std::endl;            
        };

    };

    std::vector<Entry> entries;

    static void PrintHeader() {
        std::cout 
            << std::setw(12) << "time"
            << std::setw(12) << "thread"
            << std::setw(12) << "frequency";
        for(auto& e: papi_events) {
            std::cout << std::setw(12) << e;
        }
        std::cout << std::endl;            
    }
};

struct ProcessInfo {
    struct Thread {
        int tid;
        int core;
    };

    pid_t pid;
    ProcessInfo(pid_t p) : pid(p) {}

    static std::string exec(const char* cmd) {
        std::array<char, 128> buffer;
        std::string result;
        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
        if (!pipe) {
            throw std::runtime_error("popen() failed!");
        }
        while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
            result += buffer.data();
        }
        return result;
    }

    std::vector<Thread> get_child_threads() {
        std::vector<Thread> threads;

        string cmd_get_threadId = "ps -o tid,psr -p " + std::to_string(pid) + " -T | tail -n +2";
        string str_coreId = exec(cmd_get_threadId.c_str());

        std::stringstream ss(str_coreId);
        std::string line;
        while (std::getline(ss, line)) {
            std::stringstream ss_line(line);
            int tid, core;
            ss_line >> tid;
            ss_line >> core;
            threads.emplace_back(tid, core);
        }
        return threads;
    }
};

void usage(char* argv0) {
    std::cerr << "usage: " << argv0 << " <hwloc xml path> <program_to_measure> [program params]" << std::endl;
}

int main(int argc, char *argv[])
{
    string topoPath;

    if(argc < 3)
    {
        usage(argv[0]);
        return 1;
    }
    else
    {
        topoPath = argv[1];
    }

    //create root Topology and one node
    Topology* topo = new Topology();
    Node* n = new Node(topo, 1);

    if(parseHwlocOutput(n, topoPath) != 0) { //adds topo to a next node
        usage(argv[0]);
        return 1;
    }

    pid_t child = fork();
    if(child == 0)
    {
        ptrace(PTRACE_TRACEME,0,0,0);
        int err = execvp(argv[2],&argv[2]);
        if(err)
        {
            perror("execvp");
        }
    }
    else if(child < 0)
    {
        std::cerr << "Error forking!" << std::endl;
    }
    else
    {
        // parent process, measuring child:
        int status;
        GreenScore greenScore;
        ProcessInfo processInfo{child};

        //waiting for the child process to be started...
        wait(&status);
        
        //continue child process
        ptrace(PTRACE_CONT,child,0,0);
        int process_status, finished_pid;

        std::chrono::high_resolution_clock::time_point ts, ts_start = std::chrono::high_resolution_clock::now();

        // Wait until process exits
        do {
            n->RefreshCpuCoreFrequency();
            auto threads = processInfo.get_child_threads();

            Measurement::Configuration config{papi_events};
            for(auto& threadInfo: threads) {
                config.threads.push_back(threadInfo.tid);
            }

            Measurement::init(greenScoreRegion, &config, n);        
            Measurement::start(greenScoreRegion);

            usleep(2000000);

            Measurement::stop(greenScoreRegion);
            
            ts = std::chrono::high_resolution_clock::now();
            long long time = (ts.time_since_epoch().count() - ts_start.time_since_epoch().count())/1000000;
            for(auto& threadInfo: threads) {
                Thread * hwThreadComponent = (Thread*)n->FindSubcomponentById(threadInfo.core, SYS_SAGE_COMPONENT_THREAD);
                if(hwThreadComponent==NULL) {
                    std::cerr << "Unexpected error: hw thread component with id " << threadInfo.core << " not found!" << std::endl;
                    return 1;
                }
                std::vector<long long> counters;

                Measurement::counters(greenScoreRegion, threadInfo.tid, counters);
                std::cout << "LIVE: Thread " << threadInfo.core << ", tid " << threadInfo.tid 
                    << " Time : " << time 
                    << ", Frequency: " << hwThreadComponent->GetFreq() 
                    << " ---- ";
                for(auto& counter: counters) {
                    std::cout << counter << " ";
                }
                std::cout << std::endl;
                greenScore.entries.emplace_back(time, threadInfo.core, hwThreadComponent->GetFreq(), counters);
            }
            Measurement::deinit(greenScoreRegion);
            finished_pid = waitpid(child, &process_status, WNOHANG);
        } while(finished_pid <= 0);

        GreenScore::PrintHeader();
        for(auto& entry : greenScore.entries) {
            entry.Print();
        }
    }

    return 0;
}
