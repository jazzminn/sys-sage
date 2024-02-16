#include <iostream>
#include <filesystem>
#include <unistd.h>
#include <tuple>
#include <thread>
#include <sstream>

#include "sys-sage.hpp"
#include "papi/Statistics.hpp"
#include "papi/Utility.hpp"

#include <papi.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/ptrace.h>

static std::vector<std::string> papi_events = {
    "PAPI_TOT_INS",
    "PAPI_TOT_CYC",
};
static const int papi_component = 0;

/// @brief Simple load to be analyzed
/// @param cores number of threads to start
static long loop_size = 100000000;

void prevent_optimization( void *array ) {
	( void ) array;
}

void make_load() {
    double c = 3.14;
    volatile double a = 0.5, b = 2.2;

	for(long l = 0; l < loop_size; l++) {
		c += a * b;
	}
	prevent_optimization( (void*) &c );
}

struct Speed : public SYSSAGE_PAPI_Visitor {
    std::vector<long long> values;
    long long bts = 0;
    int session = -1;
    bool data(int sid, long long sts, long long ts, int core, const std::vector<long long>& counters) {
        if ( session != sid ) {
            bts = sts;
            session = sid;
        }
        auto elapsed = ts - bts;
        if ( elapsed > 0 ) {
            std::vector<double> columns;
            std::cout << ((double)(ts - sts)/1000.0);
            int idx = 0;
            for(auto& c: counters) {
                double v = (double)(c-values[idx]) / (double)elapsed;
                std::cout << "|" << v;
                values[idx] = c;
                idx++;
            }            
            std::cout << std::endl;
            bts = ts;
        }
        return true;
    }

    void info(int eventSet, int core, unsigned long tid, const std::vector<std::string>& eventNames) {
        std::cout << "time (ms)";
        for(auto& name: eventNames) {
            std::cout << "|" << name;
        }
        std::cout << std::endl;
        values.resize(eventNames.size());
    }
};

int main(int argc, char *argv[])
{
    pid_t child_pid;

    child_pid = fork();
    if (child_pid == 0) {
        ptrace(PTRACE_TRACEME, 0, 0, 0);
        pid_t pid = getpid();
        std::cout << "Child " << pid << " started, waiting for CONT" << std::endl;
        kill(pid, SIGTRAP);
        std::cout << "Child " << pid << " CONT received" << std::endl;
        
        cpu_set_t cpu;

        CPU_ZERO(&cpu);
        CPU_SET(1, &cpu);
        sched_setaffinity(0, sizeof(cpu), &cpu);
        make_load();

        CPU_ZERO(&cpu);
        CPU_SET(3, &cpu);
        sched_setaffinity(0, sizeof(cpu), &cpu);
        make_load();

    } else if (child_pid > 0) {
        int status;

        std::cout << "Waiting for child to stop" << std::endl;
        wait(&status);

        int rv;

        // Instrument code
        // set up PAPI
        rv = PAPI_library_init(PAPI_VER_CURRENT);
        if ( rv < PAPI_OK ) {
            std::cerr << "Failed PAPI library init: " << rv << std::endl;
            exit(EXIT_FAILURE);
        }

        int eventSet = PAPI_NULL;
        rv = PAPI_create_eventset(&eventSet);
        if ( rv != PAPI_OK ) {
            std::cerr << "Failed to create event set: " << rv << std::endl;
            exit(EXIT_FAILURE);
        }
        rv = PAPI_assign_eventset_component(eventSet, papi_component);
        if ( rv != PAPI_OK ) {
            std::cerr << "Failed to create event set: " << rv << std::endl;
            exit(EXIT_FAILURE);
        }
        for(auto& name: papi_events) {
            rv = PAPI_add_named_event(eventSet, name.c_str());
            if ( rv != PAPI_OK ) {
                std::cerr << "Failed to add event " << name << " to event set: " << rv << std::endl;
                exit(EXIT_FAILURE);
            }
        }
        rv = PAPI_attach(eventSet, child_pid);
        if ( rv != PAPI_OK ) {
            std::cerr << "Failed to attach to pid " << child_pid << ": " << rv << std::endl;
            exit(EXIT_FAILURE);
        }
        rv = SYSSAGE_PAPI_start(eventSet);
        if ( rv != PAPI_OK ) {
            std::cerr << "Failed to start eventset: " << rv << std::endl;
            exit(EXIT_FAILURE);
        }

        std::cout << "Signaling child to continue" << std::endl;
        
        ptrace(PTRACE_CONT,child_pid,0,0);
        int process_status, finished_pid;

        do {
            usleep(25000);
            finished_pid = waitpid(child_pid, &process_status, WNOHANG);
            if ( finished_pid <= 0 ) {
                rv = SYSSAGE_PAPI_read(eventSet);
                if ( rv != PAPI_OK ) {
                    std::cerr << "Failed to stop and store eventset: " << rv << std::endl;
                    exit(EXIT_FAILURE);
                }
            }
        } while(finished_pid <= 0);

        rv = SYSSAGE_PAPI_stop(eventSet);
        if ( rv != PAPI_OK ) {
            std::cerr << "Failed to stop and store eventset: " << rv << std::endl;
            exit(EXIT_FAILURE);
        }

        SYSSAGE_PAPI_print(eventSet);

        std::cout << "-Growth Speed Example---------------------------------" << std::endl;
        Speed speedCalculator;
        SYSSAGE_PAPI_visit(eventSet, speedCalculator);

        papi::StatisticsHandler stats;
        SYSSAGE_PAPI_visit(eventSet, stats);

        SYSSAGE_PAPI_destroy_eventset(&eventSet);

        std::cout << "-Statstics-Example---------------------------------" << std::endl;
        papi::Printer::print(stats.frozen(), std::cout, 20);
    }
    return EXIT_SUCCESS;
}
