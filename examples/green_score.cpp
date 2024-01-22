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

#include <papi.h>

#include "sys-sage.hpp"

static std::vector<std::string> papi_events = {
    "PAPI_TOT_INS",
    "PAPI_TLB_DM",
    "PAPI_TOT_CYC",
    "PAPI_BR_INS",
    //"PAPI_L2_TCA",
    //"PAPI_L3_LDM",
    //"PAPI_L3_TCM"
};
static const int papi_component = 0;

struct GreenScore {
    struct Entry {
        uint64_t time;
        int hwThread;
        double frequency;
        std::vector<long long> papi_counters;

        void Print()
        {
            std::cout 
                << std::setw(16) << time 
                << std::setw(16) << hwThread
                << std::setw(16) << frequency;
            for(auto& c: papi_counters) {
                std::cout << std::setw(16) << c;
            }
            std::cout << std::endl;            
        };

    };

    std::vector<Entry> entries;

    static void PrintHeader() {
        std::cout 
            << std::setw(16) << "time"
            << std::setw(16) << "thread"
            << std::setw(16) << "frequency";
        for(auto& e: papi_events) {
            std::cout << std::setw(16) << e;
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
        int rv;
        GreenScore greenScore;
        ProcessInfo processInfo{child};

        //waiting for the child process to be started...
        wait(&status);

        // set up PAPI
        rv = PAPI_library_init(PAPI_VER_CURRENT);
        if ( rv < PAPI_OK ) {
            std::cerr << "Failed PAPI library init: " << rv << std::endl;
            kill(child, SIGKILL);
            exit(EXIT_FAILURE);
        }

        //continue child process
        ptrace(PTRACE_CONT,child,0,0);
        int process_status, finished_pid;

        std::chrono::high_resolution_clock::time_point ts, ts_start = std::chrono::high_resolution_clock::now();

        // Wait until process exits
        do {
            n->RefreshCpuCoreFrequency();
            auto threads = processInfo.get_child_threads();
            auto num_threads = threads.size();
            if ( num_threads == 0 ) {
                std::cerr << "No threads found in child process." << std::endl;
                usleep(1000000);
                continue;
            }
            int eventSets[num_threads];

            // create event sets for each threads
            for(size_t i=0; i<num_threads; ++i) {
                int eventSet = PAPI_NULL;
                rv = PAPI_create_eventset(&eventSet);
                if ( rv != PAPI_OK ) {
                    std::cerr << "Failed to create event set: " << rv << std::endl;
                    kill(child, SIGKILL);
                    exit(EXIT_FAILURE);
                }
                rv = PAPI_assign_eventset_component(eventSet, papi_component);
                if ( rv != PAPI_OK ) {
                    std::cerr << "Failed to create event set: " << rv << std::endl;
                    kill(child, SIGKILL);
                    exit(EXIT_FAILURE);
                }
                for(auto& name: papi_events) {
                    rv = PAPI_add_named_event(eventSet, name.c_str());
                    if ( rv != PAPI_OK ) {
                        std::cerr << "Failed to add event " << name << " to event set: " << rv << std::endl;
                        kill(child, SIGKILL);
                        exit(EXIT_FAILURE);
                    }
                }

                // finally attach eventset to thread
                rv = PAPI_attach(eventSet, threads[i].tid);
                if ( rv != PAPI_OK ) {
                    std::cerr << "Failed to attach event set to tid " << threads[i].tid << ": " << rv << std::endl;
                    kill(child, SIGKILL);
                    exit(EXIT_FAILURE);
                }
                eventSets[i] = eventSet;
            }

            // all eventsets are intialized
            for(auto eventSet: eventSets) {
                rv = PAPI_start(eventSet);
                if ( rv != PAPI_OK ) {
                    std::cerr << "Failed to start eventset: " << rv << std::endl;
                    kill(child, SIGKILL);
                    exit(EXIT_FAILURE);
                }
            }

            usleep(2000000);

            ts = std::chrono::high_resolution_clock::now();
            long long time = (ts.time_since_epoch().count() - ts_start.time_since_epoch().count())/1000000;
            for(size_t i=0; i<num_threads; ++i) {
                Thread * hwThreadComponent = (Thread*)n->FindSubcomponentById(threads[i].core, SYS_SAGE_COMPONENT_THREAD);
                if(hwThreadComponent==NULL) {
                    std::cerr << "Unexpected error: hw thread component with id " << threads[i].core << " not found!" << std::endl;
                    kill(child, SIGKILL);
                    exit(EXIT_FAILURE);
                }

                rv = hwThreadComponent->PAPI_stop(eventSets[i]);
                if ( rv != PAPI_OK ) {
                    std::cerr << "Failed to stop and store eventset: " << rv << std::endl;
                    kill(child, SIGKILL);
                    exit(EXIT_FAILURE);
                }
                std::vector<long long> counters;
                rv = hwThreadComponent->PAPI_lastCounters(counters);
                if ( rv != PAPI_OK ) {
                    std::cerr << "Failed to get last counters: " << rv << std::endl;
                    kill(child, SIGKILL);
                    exit(EXIT_FAILURE);
                }

                std::cout << "LIVE: Thread " << threads[i].core << ", tid " << threads[i].tid 
                    << " Time : " << time 
                    << ", Frequency: " << hwThreadComponent->GetFreq() 
                    << " ---- ";
                for(auto& counter: counters) {
                    std::cout << counter << " ";
                }
                std::cout << std::endl;
                greenScore.entries.emplace_back(time, threads[i].core, hwThreadComponent->GetFreq(), counters);
                
                PAPI_destroy_eventset(&(eventSets[i]));
            }

            finished_pid = waitpid(child, &process_status, WNOHANG);
        } while(finished_pid <= 0);

        GreenScore::PrintHeader();
        for(auto& entry : greenScore.entries) {
            entry.Print();
        }

        // using component storage
        std::vector<Component*> threads;
        n->FindAllSubcomponentsByType(&threads, SYS_SAGE_COMPONENT_THREAD);
        for(auto component: threads) {
            Thread * thread = (Thread*)component;
            auto countersList = thread->PAPI_getCounters();
            std::cout << "Thread " << thread->GetId() << std::endl;
            for(auto& counters: countersList) {
                for(auto value: counters) {
                    std::cout << value << "|";
                }
                std::cout << std::endl;
            }
        }
    }

    return 0;
}
