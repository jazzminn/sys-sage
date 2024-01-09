#pragma once

#include "Topology.hpp"
#include "Measurement.hpp"

#include "papi/Region.hpp"
#include "papi/Logging.hpp"

#include <fstream>

namespace papi {

/// @brief Factory responsibe for managing metrics regions
/// also initializes the PAPI library if needed.
struct MetricsFactory {
    std::map<std::string, Region> regions;
    int regionsCreated = 0;

    bool has(const std::string& region);

    bool create(const std::string& name, Measurement::Configuration* configuration, Component* component);

    bool initMetricsLibrary();

    bool checkSystemConfiguration();
};

}