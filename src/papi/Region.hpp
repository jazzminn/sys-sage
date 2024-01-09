#pragma once

#include "Topology.hpp"
#include "Measurement.hpp"

#include "papi/EventSetManager.hpp"

namespace papi {

//
// Metrics Management
//
struct Region {
    // input members
    std::string name;
    Measurement::Configuration* configuration;
    Component* topology;

    EventSetManager eventSetManager;

    int init(int cpuId = -1);

    bool component_has_hwthread(Component* component, int hwThreadId);

    int deinit();
    int start();
    int read();
    int stop();
    int save();
};

}