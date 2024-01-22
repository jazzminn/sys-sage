#pragma once

#include <string>
#include <vector>
#include <map>
#include <papi.h>

#include "Topology.hpp"
#include "Measurement.hpp"

namespace papi {

//
// PAPI wrapper
//
struct CounterReading {
    long long timestamp;
    long long value;
};
struct ComponentEvent {
    int id;
    std::string name;
    Component* component;
    std::vector<CounterReading> readings;
};

struct EventSet {
    int papiEventSet;
    int papiComponent;
    int cpu = -1; // -1, no CPU ATTACHED
    int tid = 0;
    long long startTimeStamp;
    long long stopTimeStamp;
    std::vector<long long> counters;
    std::vector<ComponentEvent> events;
};

struct MetricsResults {
    long long elapsed;
    long long counter;
};
struct ComponentMetricsResults {
    std::map<std::string, std::vector<MetricsResults>> eventMetrics;
};

struct PapiParams {
    short component = 0;
    short cpu = -1;
    int tid = -1;
};

union EventSetId {
    //int compCpu[2];
    PapiParams papi;
    long id;
};

struct EventSetManager {
    std::map<long, EventSet> eventSets;

    int registerEvent(Component* component, const std::string& eventName, int eventId, int cpuId, int tid = 0);

    int startAll();

    int stopAll();

    int saveAll();

    int populateCountersForThread(int tid, std::vector<long long>& counters);

    virtual ~EventSetManager() {
        //TODO implement destructor
        // delete event set, free allocated objects
    }

    static int attribXmlHandler(std::string key, void* value, xmlNodePtr n);
};

}