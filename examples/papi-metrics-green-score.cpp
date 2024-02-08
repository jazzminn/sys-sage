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
#include "papi/SystemInfo.hpp"

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

struct GreenScore : public SYSSAGE_PAPI_Visitor {
    struct Entry {
        uint64_t time;
        int hwThread;
        double frequency;
        std::vector<long long> papi_counters;
        Entry(uint64_t t, int h, double f, std::vector<long long> c)
            : time{t}, hwThread{h}, frequency{f}, papi_counters{c} {}

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

    int cpu;
    double cpuFrequency;
    long long time;
    void use(long long t, Component* component) {
        cpu = -1;
        cpuFrequency = 0.0;
        time = t;
        if ( component->GetComponentType() == SYS_SAGE_COMPONENT_THREAD ) {
            Thread* hwThreadComponent = (Thread*)component;
            cpuFrequency = hwThreadComponent->GetFreq();
            cpu = hwThreadComponent->GetId();
        }        
    }
    bool data(int sid, long long sts, long long ts, int core, const std::vector<long long>& c) {
        entries.emplace_back(time, cpu, cpuFrequency, c);
        return true;
    }
    void info(int eventSet, int core, unsigned long tid, const std::vector<std::string>& eventNames) {        
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
            auto tids = papi::SystemInfo::listThreads(child);
            auto num_threads = tids.size();
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
                rv = PAPI_attach(eventSet, tids[i]);
                if ( rv != PAPI_OK ) {
                    std::cerr << "Failed to attach event set to tid " << tids[i] << ": " << rv << std::endl;
                    kill(child, SIGKILL);
                    exit(EXIT_FAILURE);
                }
                eventSets[i] = eventSet;
            }

            // all eventsets are intialized
            for(auto eventSet: eventSets) {
                rv = SYSSAGE_PAPI_start(eventSet);
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
                Component* component;
                rv = SYSSAGE_PAPI_stop(eventSets[i], topo, &component);
                if ( rv != PAPI_OK ) {
                    std::cerr << "Failed to stop and store eventset: " << rv << std::endl;
                    kill(child, SIGKILL);
                    exit(EXIT_FAILURE);
                }
                greenScore.use(time, component);
                rv = SYSSAGE_PAPI_visit(eventSets[i], greenScore);

                SYSSAGE_PAPI_destroy(eventSets[i]);
            }

            finished_pid = waitpid(child, &process_status, WNOHANG);
        } while(finished_pid <= 0);

        GreenScore::PrintHeader();
        for(auto& entry : greenScore.entries) {
            entry.Print();
        }
    }

    return 0;
}
