#include "papi/MetricsFactory.hpp"
#include "papi/Logging.hpp"

static const char* filePerfEventParanoid = "/proc/sys/kernel/perf_event_paranoid";

using namespace papi;

bool MetricsFactory::has(const std::string& region) {
    return regions.count(region) != 0;
}

bool MetricsFactory::create(const std::string& name, Measurement::Configuration* configuration, Component* component) {
    if ( regionsCreated == 0 ) {
        if ( !initMetricsLibrary() ) return false;
    }
    regions.emplace(std::make_pair(name, Region{name, configuration, component}));
    regionsCreated++;
    return true;
}

bool MetricsFactory::initMetricsLibrary() {
    int rv = PAPI_library_init(PAPI_VER_CURRENT);
    if ( rv < PAPI_OK ) {
        logprintf("Failed PAPI library init: %d", rv);
        return false;
    }
    //TODO rv = PAPI_multiplex_init();
    return checkSystemConfiguration();
}

bool MetricsFactory::checkSystemConfiguration() {
    std::ifstream settingsFile{filePerfEventParanoid};
    if ( settingsFile.is_open() ) {
        int level;
        if ( settingsFile >> level ) {
            if ( level == -1 ) {
                return true;
            }
            logprintf("perf_event_paranoid is %d, required: -1", level);
        } else {
            logprintf("Failed to read perf_event_paranoid value");
        }
    } else {
        logprintf("Failed to open perf_event_paranoid file");
    }
    return false;
}
