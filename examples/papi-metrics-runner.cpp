#include <pthread.h>
#include <sched.h>
#include <errno.h>
#include <unistd.h>
#include <sys/sysinfo.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <papi.h>

#include <iostream>

#include "sys-sage.hpp"
#include "papi/SystemInfo.hpp"

using namespace papi;

static int papi_component = 0;
static int check_delay = 1000; // in microseconds

struct HwThreadEventSet {
    Thread* threadComponent;
    int eventSet;
};

void usage(char* argv0) {
    std::cerr << "usage: " << argv0 << " hwloc.xml program [optional arguments for program]" << std::endl;
    return;
}

void measure(pid_t child_pid, Node* n, const std::vector<std::string>& events) {
    int rv;
    int status;

    std::vector<HwThreadEventSet> hwThreadEventSets;
    std::map<int, Thread*> tidToHwEventSet;

    do {
        auto child_threads = SystemInfo::listThreads(child_pid);

        for(auto tid: child_threads) {
            if ( tidToHwEventSet.count(tid) > 0 ) continue;

            // setup eventset for thread
            int core = SystemInfo::getThreadCpu(tid);
            std::cout << "New thread " << tid << " on " << core << std::endl;

            int eventSet = PAPI_NULL;
            rv = PAPI_create_eventset(&eventSet);
            if ( rv != PAPI_OK ) {
                std::cerr << "Failed to create event set: " << rv << std::endl;
                kill(child_pid, SIGKILL);
                exit(EXIT_FAILURE);
            }
            rv = PAPI_assign_eventset_component(eventSet, papi_component);
            if ( rv != PAPI_OK ) {
                std::cerr << "Failed to assign component for event set: " << rv << std::endl;
                kill(child_pid, SIGKILL);
                exit(EXIT_FAILURE);
            }
            for(auto& name: events) {
                rv = PAPI_add_named_event(eventSet, name.c_str());
                if ( rv != PAPI_OK ) {
                    std::cerr << "Failed to add event " << name << " to event set: " << rv << std::endl;
                    exit(EXIT_FAILURE);
                }
            }

            rv = PAPI_attach(eventSet, tid);
            if ( rv != PAPI_OK ) {
                std::cerr << "Failed to attach event set to tid: " << rv << std::endl;
                kill(child_pid, SIGKILL);
                exit(EXIT_FAILURE);
            }

            Thread * hwThreadComponent = (Thread*)n->FindSubcomponentById(core, SYS_SAGE_COMPONENT_THREAD);
            if(hwThreadComponent==NULL) {
                std::cerr << "Unexpected error: hw thread component with id " << core << " not found!" << std::endl;
                kill(child_pid, SIGKILL);
                exit(EXIT_FAILURE);
            }
            if ( !hwThreadComponent->PAPI_storageInitialized() ) {
                rv = hwThreadComponent->PAPI_initializeStorage(eventSet);
                if ( rv != PAPI_OK ) {
                    std::cerr << "Failed to start event set measurement in component: " << rv << std::endl;
                    kill(child_pid, SIGKILL);
                    exit(EXIT_FAILURE);
                }
            }
            hwThreadEventSets.emplace_back(hwThreadComponent, eventSet);
            tidToHwEventSet[tid] = hwThreadComponent;

            rv = PAPI_start(eventSet);
            if ( rv != PAPI_OK ) {
                std::cerr << "Failed to start event set: " << rv << std::endl;
                kill(child_pid, SIGKILL);
                exit(EXIT_FAILURE);
            }
        }

        usleep(check_delay);

        // check child status
        rv = waitpid(child_pid, &status, WNOHANG);
    } while(rv <= 0);

    for(auto& hwtes: hwThreadEventSets) {
        rv = hwtes.threadComponent->PAPI_stop(hwtes.eventSet);
        if ( rv != PAPI_OK ) {
            std::cerr << "Failed to start stop set: " << rv << std::endl;
            kill(child_pid, SIGKILL);
            exit(EXIT_FAILURE);
        }
        PAPI_destroy_eventset(&hwtes.eventSet);
    }
}

int main(int argc, char *argv[]) {
    int rv;
    string output_name = "sys-sage_papi-metrics.xml";

    if (argc < 3) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }
    string topoPath{ argv[1] };
    string execPath{ argv[2] };

    pid_t child_pid = fork();
    if ( child_pid < 0 ) {
        std::cerr << "Could not fork, err: " << errno << std::endl;        
        return EXIT_FAILURE;
    }
    if (child_pid == 0) {
        // child executing target program
        ptrace(PTRACE_TRACEME, 0, 0, 0);
        execvp(argv[2], &argv[2]);
        // if execvp is successfully started, then lines below
        // will be never executed, so report an whenever reached:
        perror("Failed to execute program");
        return EXIT_FAILURE;
    }

    int status;

    // wait for child to exec'd
    wait(&status);
    if ( !WIFSTOPPED(status) ) {
        std::cerr << "Unexpected signal from child: " << status << std::endl;
        kill(child_pid, SIGKILL);
        return EXIT_FAILURE;
    }

    // parent process, measuring program started by child...
    // get measurement configuration from environment
    auto events = Component::PAPI_EventsFromEnvironment();
    if ( events.size() == 0 ) {
        std::cerr << "No Sys-Sage PAPI events configured, please set environment variable SYS_SAGE_METRICS" << std::endl;
        kill(child_pid, SIGKILL);
        return EXIT_FAILURE;
    }

    // build topology
    Topology* topo = new Topology();
    Node* n = new Node(topo, 1);

    if(parseHwlocOutput(n, topoPath) != 0) {
        std::cerr << "Failed to build topology." << std::endl;
        kill(child_pid, SIGKILL);
        return EXIT_FAILURE;
    }

    // set up PAPI
    rv = PAPI_library_init(PAPI_VER_CURRENT);
    if ( rv < PAPI_OK ) {
        std::cerr << "Failed PAPI library init: " << rv << std::endl;
        kill(child_pid, SIGKILL);
        exit(EXIT_FAILURE);
    }

    // child has successfully started execPath program
    long lrv = ptrace(PTRACE_CONT, child_pid, 0, 0);
    if ( lrv != 0 ) {
        std::cerr << "Failed to CONT ptrace: " << lrv << std::endl;
        kill(child_pid, SIGKILL);
        exit(EXIT_FAILURE);
    }

    //execute application measurement
    //note: this function will wait child to be terminated
    measure(child_pid, n, events);

    // export topology with measurement results
    bool success = exportToXml(topo, output_name, Component::PAPI_attribHandler, Component::PAPI_attribXmlHandler) == PAPI_OK;

    delete topo;
    delete n;

    return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
