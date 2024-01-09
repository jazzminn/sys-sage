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

union EventSetId {
    int compCpu[2];
    long id;
};

struct EventSetManager {
    std::map<long, EventSet> eventSets;

    int registerEvent(Component* component, const std::string& eventName, int eventId, int cpuId);

    int startAll();

    int stopAll();

    int saveAll();

    virtual ~EventSetManager() {
        //TODO implement destructor
        // delete event set, free allocated objects
    }

    static int attribXmlHandler(std::string key, void* value, xmlNodePtr n);
};

}