#include <iostream>
#include <chrono>
#include <functional>
#include <sstream>

#include <papi.h>
#include "sys-sage.hpp"
#include "papi/Statistics.hpp"

static std::vector<std::string> papi_events = {
    "PAPI_TOT_INS",
    "PAPI_TOT_CYC",
    "PAPI_L1_DCM",
    "PAPI_L1_ICM",
    "PAPI_L2_DCM",
    "PAPI_L2_ICM",
    "PAPI_L3_TCM",
    "PAPI_L3_LDM",
    "PAPI_STL_CCY",
};
static uint64_t test_count = 100;
static size_t event_count = papi_events.size();
static uint64_t overhead = 0;

using MeasuredFunction = std::function<void(void)>;

void dummy(uint64_t start, uint64_t stop) {

}

uint64_t get_timer_overhead(int repeats, int warmup) {
    std::chrono::high_resolution_clock::time_point t_start, t_end;
    uint64_t time = 0;

    for(int i=0; i<warmup; i++) {
        t_start = std::chrono::high_resolution_clock::now();
        t_end = std::chrono::high_resolution_clock::now();
        dummy(t_start.time_since_epoch().count(), t_end.time_since_epoch().count());
    }
    
    for(int i=0; i<repeats; i++) {
        t_start = std::chrono::high_resolution_clock::now();
        t_end = std::chrono::high_resolution_clock::now();
        time += std::chrono::duration_cast<std::chrono::nanoseconds>(t_end - t_start).count();
    }
    time = time/repeats;
    return time;
}

uint64_t benchmark_papi(const std::string& name, MeasuredFunction fn) {
    std::chrono::high_resolution_clock::time_point t_start, t_end;
    t_start = std::chrono::high_resolution_clock::now();
    fn();
    t_end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(t_end - t_start).count();
}

void build_eventset(int& eventSet) {
    int rv = PAPI_create_eventset(&eventSet);
    if ( rv != PAPI_OK ) {
        std::cerr << "Failed to create event set: " << rv << std::endl;
        exit(EXIT_FAILURE);
    }

    int component = -1;
    uint64_t events_added = 0;
    for(auto& name: papi_events) {
        int eventId = PAPI_NULL;

        rv = PAPI_event_name_to_code(name.c_str(), &eventId);
        if ( rv < PAPI_OK ) {
            std::cerr << "Failed to get event name: " << rv << std::endl;
            exit(EXIT_FAILURE);
        }

        rv = PAPI_get_event_component(eventId);
        if ( rv < PAPI_OK ) {
            std::cerr << "Failed to get event component: " << rv << std::endl;
            exit(EXIT_FAILURE);
        }

        if ( component == -1 ) {
            component = rv;
            //std::cout << "PAPI component for " << name << "=" << component << std::endl;
            rv = PAPI_assign_eventset_component(eventSet, component);
            if ( rv != PAPI_OK ) {
                std::cerr << "Failed to set component: " << rv << std::endl;
                exit(EXIT_FAILURE);
            }
        }

        rv = PAPI_add_event(eventSet, eventId);
        if ( rv != PAPI_OK ) {
            std::cerr << "Failed to add event " << name << " : " << rv << std::endl;
            exit(EXIT_FAILURE);
        }

        events_added++;
        if ( events_added == event_count ) break;
    }
}

void test_papi() {
    int eventSet = PAPI_NULL;
    build_eventset(eventSet);

    std::cout << "Running PAPI start-read-stop loop" << std::endl;
    std::cout << " - Repeat count: " << test_count << std::endl;
    std::cout << " - Event count: " << event_count << std::endl;
    std::cout << " - Timer overhead: " << overhead << " ns" << std::endl;

    std::vector<uint64_t> start_time;
    std::vector<uint64_t> read_time;
    std::vector<uint64_t> stop_time;
    for(uint64_t i=0; i<test_count; i++) {
        int rv;
        auto duration = benchmark_papi("PAPI_start", [&]() {
            rv = PAPI_start(eventSet);
        });
        if ( rv != PAPI_OK ) {
            std::cerr << "Failed to start eventset: " << rv << std::endl;
            exit(EXIT_FAILURE);
        }
        start_time.push_back(duration-overhead);

        duration = benchmark_papi("PAPI_read", [&]() {
            long long values[event_count];
            rv = PAPI_read(eventSet, values);
        });
        if ( rv != PAPI_OK ) {
            std::cerr << "Failed to start eventset: " << rv << std::endl;
            exit(EXIT_FAILURE);
        }
        read_time.push_back(duration-overhead);

        duration = benchmark_papi("PAPI_stop", [&]() {
            long long * values = new long long[event_count];
            rv = PAPI_stop(eventSet, values);
            delete [] values;
        });
        if ( rv != PAPI_OK ) {
            std::cerr << "Failed to start eventset: " << rv << std::endl;
            exit(EXIT_FAILURE);
        }
        stop_time.push_back(duration-overhead);
    }

    std::cout << "Start: ";
    papi::Statistics<uint64_t>::calculate(start_time).print();
    std::cout << "Read: ";
    papi::Statistics<uint64_t>::calculate(read_time).print();
    std::cout << "Stop: ";
    papi::Statistics<uint64_t>::calculate(stop_time).print();

    PAPI_destroy_eventset(&eventSet);
}

void test_syssage_papi() {
    int eventSet = PAPI_NULL;
    build_eventset(eventSet);

    std::cout << "Running SYSSAGE PAPI start-read-stop loop" << std::endl;
    std::cout << " - Repeat count: " << test_count << std::endl;
    std::cout << " - Event count: " << event_count << std::endl;
    std::cout << " - Timer overhead: " << overhead << " ns" << std::endl;

    std::vector<uint64_t> start_time;
    std::vector<uint64_t> read_time;
    std::vector<uint64_t> stop_time;
    for(uint64_t i=0; i<test_count; i++) {
        int rv;
        auto duration = benchmark_papi("PAPI_start", [&]() {
            rv = SYSSAGE_PAPI_start(eventSet);
        });
        if ( rv != PAPI_OK ) {
            std::cerr << "Failed to start eventset: " << rv << std::endl;
            exit(EXIT_FAILURE);
        }
        start_time.push_back(duration-overhead);

        duration = benchmark_papi("PAPI_read", [&]() {
            rv = SYSSAGE_PAPI_read(eventSet);
        });
        if ( rv != PAPI_OK ) {
            std::cerr << "Failed to start eventset: " << rv << std::endl;
            exit(EXIT_FAILURE);
        }
        read_time.push_back(duration-overhead);

        duration = benchmark_papi("PAPI_stop", [&]() {
            rv = SYSSAGE_PAPI_stop(eventSet);
        });
        if ( rv != PAPI_OK ) {
            std::cerr << "Failed to start eventset: " << rv << std::endl;
            exit(EXIT_FAILURE);
        }
        stop_time.push_back(duration-overhead);
    }

    std::cout << "Start: ";
    papi::Statistics<uint64_t>::calculate(start_time).print();
    std::cout << "Read: ";
    papi::Statistics<uint64_t>::calculate(read_time).print();
    std::cout << "Stop: ";
    papi::Statistics<uint64_t>::calculate(stop_time).print();

    SYSSAGE_PAPI_destroy_eventset(&eventSet);
}

int main(int argc, char *argv[]) {
    overhead = get_timer_overhead(100, 10);
    std::cout << "SYS-SAGE PAPI benchmarks." << std::endl;

    if ( argc > 1 ) {
        std::stringstream sstream{argv[1]};
        sstream >> event_count;
        if ( event_count > papi_events.size() ) event_count = papi_events.size();
    }
    if ( argc > 2 ) {
        std::stringstream sstream{argv[2]};
        sstream >> test_count;
    }
    
    int rv = PAPI_library_init(PAPI_VER_CURRENT);
    if ( rv < PAPI_OK ) {
        std::cerr << "Failed library init: " << rv << std::endl;
        exit(EXIT_FAILURE);
    }

    test_papi();

    test_syssage_papi();
}