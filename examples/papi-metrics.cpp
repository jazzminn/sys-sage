#include <iostream>
#include <filesystem>
#include <unistd.h>
#include <tuple>
#include <thread>

#include "sys-sage.hpp"
#include <papi.h>
#include <sched.h>
#include <errno.h>

static std::vector<std::string> papi_events = {
    "PAPI_TOT_INS",
    "PAPI_TOT_CYC",
};
static const int papi_component = 0;

/// @brief Simple load to be analyzed
/// @param cores number of threads to start
static long loop_size = 1000000000;

void make_load() {
    long v = 0;
    for(long l = 0; l < loop_size; l++) {
        v += l;
    }
}

void usage(char* argv0)
{
    std::cerr << "usage: " << argv0 << " hwloc_xml_path [xml output path/name]" << std::endl;
    return;
}

int main(int argc, char *argv[])
{
    int rv;
    string topoPath;
    string output_name = "sys-sage_papi-metrics.xml";
    
    if(argc == 2){
        topoPath = argv[1];
    } else if(argc == 3){
        topoPath = argv[1];
        output_name = argv[2];
    } else {
        usage(argv[0]);
        return 1;
    }

    Topology* topo = new Topology();
    Node* n = new Node(topo, 1);

    if(parseHwlocOutput(n, topoPath) != 0) { //adds topo to a next node
        usage(argv[0]);
        return 1;
    }

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

    int core = sched_getcpu();
    if ( core == -1 ) {
        std::cerr << "Failed to determine virtual core: " << errno << std::endl;
        exit(EXIT_FAILURE);
    }

    Thread * hwThreadComponent = (Thread*)n->FindSubcomponentById(core, SYS_SAGE_COMPONENT_THREAD);
    if(hwThreadComponent==NULL) {
        std::cerr << "Unexpected error: hw thread component with id " << core << " not found!" << std::endl;
        exit(EXIT_FAILURE);
    }

    rv = hwThreadComponent->PAPI_initializeStorage(eventSet);
    if ( rv != PAPI_OK ) {
        std::cerr << "Failed to start eventset: " << rv << std::endl;
        exit(EXIT_FAILURE);
    }

    rv = PAPI_start(eventSet);
    if ( rv != PAPI_OK ) {
        std::cerr << "Failed to start eventset: " << rv << std::endl;
        exit(EXIT_FAILURE);
    }

    make_load();

    rv = hwThreadComponent->PAPI_read(eventSet);
    if ( rv != PAPI_OK ) {
        std::cerr << "Failed to stop and store eventset: " << rv << std::endl;
        exit(EXIT_FAILURE);
    }

    make_load();

    rv = hwThreadComponent->PAPI_stop(eventSet);
    if ( rv != PAPI_OK ) {
        std::cerr << "Failed to stop and store eventset: " << rv << std::endl;
        exit(EXIT_FAILURE);
    }

    PAPI_destroy_eventset(&eventSet);

    bool success = exportToXml(topo, output_name, Component::PAPI_attribHandler, Component::PAPI_attribXmlHandler) == PAPI_OK;

    delete topo;
    delete n;

    return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
