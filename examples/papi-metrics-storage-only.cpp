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
static long loop_size = 10000000;

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

int main(int argc, char *argv[])
{
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

    rv = SYSSAGE_PAPI_start(eventSet);
    if ( rv != PAPI_OK ) {
        std::cerr << "Failed to start eventset: " << rv << std::endl;
        exit(EXIT_FAILURE);
    }

    make_load();

    rv = SYSSAGE_PAPI_read(eventSet);
    if ( rv != PAPI_OK ) {
        std::cerr << "1 Failed to stop and store eventset: " << rv << std::endl;
        exit(EXIT_FAILURE);
    }

    make_load();

    rv = SYSSAGE_PAPI_stop(eventSet);
    if ( rv != PAPI_OK ) {
        std::cerr << "2 Failed to stop and store eventset: " << rv << std::endl;
        exit(EXIT_FAILURE);
    }

    rv = SYSSAGE_PAPI_start(eventSet);
    if ( rv != PAPI_OK ) {
        std::cerr << "Failed to start eventset: " << rv << std::endl;
        exit(EXIT_FAILURE);
    }

    make_load();

    rv = SYSSAGE_PAPI_read(eventSet);
    if ( rv != PAPI_OK ) {
        std::cerr << "3 Failed to stop and store eventset: " << rv << std::endl;
        exit(EXIT_FAILURE);
    }

    rv = SYSSAGE_PAPI_start(eventSet);
    if ( rv != PAPI_EISRUN ) {
        std::cerr << "Error: duplicated start returns value other than " << PAPI_EISRUN << ": " << rv << std::endl;
        exit(EXIT_FAILURE);
    }

    make_load();

    rv = SYSSAGE_PAPI_stop(eventSet);
    if ( rv != PAPI_OK ) {
        std::cerr << "4 Failed to stop and store eventset: " << rv << std::endl;
        exit(EXIT_FAILURE);
    }

    SYSSAGE_PAPI_print(eventSet);

    SYSSAGE_PAPI_destroy_eventset(&eventSet);

    return EXIT_SUCCESS;
}
