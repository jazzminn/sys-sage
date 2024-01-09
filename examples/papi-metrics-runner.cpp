#include <pthread.h>
#include <sched.h>
#include <errno.h>
#include <unistd.h>
#include <sys/sysinfo.h>
#include <sys/ptrace.h>
#include <sys/wait.h>

#include <iostream>

#include "sys-sage.hpp"

void usage(char* argv0)
{
    std::cerr << "usage: " << argv0 << " hwloc.xml program [optional arguments for program]" << std::endl;
    return;
}

int main(int argc, char *argv[])
{
    bool success = false;
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
        perror("Failed to execute program");
        return EXIT_FAILURE;
    }

    // parent process, measuring program started by child...

    // get measurement configuration from environment
    // the configuration is constant, cannot be changed during measurement
    auto configuration = Measurement::Configuration::fromEnvironment();

    // build topology
    Topology* topo = new Topology();
    Node* n = new Node(topo, 1);

    if(parseHwlocOutput(n, topoPath) != 0) {
        std::cerr << "Failed to build topology." << std::endl;

        // try to terminate child
        kill(SIGTERM, child_pid);
        int status;
        wait(&status);
        return EXIT_FAILURE;
    }

    // initialize the measurement
    // - an internal measurement object is created, stored with key execPath in a map
    // - the PAPI event sets initialized, based on the configuration
    int rv = Measurement::init(execPath, &configuration, topo);
    if ( rv == STATUS_OK ) {
        // wait child to stop
        int status;
        wait(&status);
        if ( WIFSTOPPED(status) ) {
            // child has successfully started execPath program
            long lrv = ptrace(PTRACE_CONT, child_pid, 0, 0);
            if ( lrv == 0 ) {
                // start measurements
                // - start all PAPI event set
                rv = Measurement::start(execPath);
                if ( rv == STATUS_OK ) {
                    // wait for child termination
                    do {
                        wait(&status);
                    } while(!WIFEXITED(status));
                    // execPath program exited
                    // let us collect all counter values
                    // and store them to topology attribures
                    rv = Measurement::end(execPath);
                    if ( rv == STATUS_OK ) {
                        // all OK, export topology with measurement results
                        success = exportToXml(topo, output_name) == STATUS_OK;
                    }
                }
            }
        }
    }

    //TODO terminate child if necessary
    if ( !success ) {
        kill(SIGTERM, child_pid);
    }
    
    // the internal measurement object will be destructed
    Measurement::deinit(execPath);

    delete topo;
    delete n;

    return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
