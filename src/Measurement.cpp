#include "Measurement.hpp"

#include "papi/EventSetManager.hpp"
#include "papi/Region.hpp"
#include "papi/MetricsFactory.hpp"
#include "papi/Logging.hpp"

#include <map>
#include <regex>

#include <sched.h>

using namespace papi;

//
// Constants and Variables
//
static const char* envMetricsConfigKey = "SYS_SAGE_METRICS";

static MetricsFactory factory;

//
// Mapping
//
Measurement::ComponentInfo Measurement::getComponentForEvent(const std::string& eventName) {
    std::map<std::string, Measurement::ComponentInfo> rules = {
        // mapping in this order
        {"PAPI_REF_CYC", {SYS_SAGE_COMPONENT_THREAD}}, // override CY mapping, s.u.
        {"RAPL", {SYS_SAGE_COMPONENT_NODE}}, // uncore events
        {"L3", {SYS_SAGE_COMPONENT_CACHE, 3}},
        {"L2", {SYS_SAGE_COMPONENT_CACHE, 2}},
        {"L1", {SYS_SAGE_COMPONENT_CACHE, 1}},
        {"LLC", {SYS_SAGE_COMPONENT_CACHE, 3}},
        {"INS", {SYS_SAGE_COMPONENT_CORE}},
        {"CY", {SYS_SAGE_COMPONENT_CORE}},
    };
    for(auto& entry: rules) {
        if ( eventName.find(entry.first) != std::string::npos ) {
            return entry.second;
        }
    }
    return {SYS_SAGE_COMPONENT_NODE};
}

bool Measurement::ComponentInfo::override(const std::string& option) {
    std::map<std::string, Measurement::ComponentInfo> rules = {
        {"NODE", {SYS_SAGE_COMPONENT_NODE}},
        {"CHIP", {SYS_SAGE_COMPONENT_CHIP}},
        {"CORE", {SYS_SAGE_COMPONENT_CORE}},
        {"NUMA", {SYS_SAGE_COMPONENT_NUMA}},
        {"MEMORY", {SYS_SAGE_COMPONENT_MEMORY}},
        {"STORAGE", {SYS_SAGE_COMPONENT_STORAGE}},
        {"THREAD", {SYS_SAGE_COMPONENT_THREAD}},
        {"SUBDIVISION", {SYS_SAGE_COMPONENT_SUBDIVISION}},
        {"L3", {SYS_SAGE_COMPONENT_CACHE, 3}},
        {"L2", {SYS_SAGE_COMPONENT_CACHE, 2}},
        {"L1", {SYS_SAGE_COMPONENT_CACHE, 1}},
    };
    std::string o{option};
    for (auto & c: o) c = std::toupper(c);
    for(auto& entry: rules) {
        if ( entry.first.compare(o) == 0 ) {
            componentType = entry.second.componentType;
            level = entry.second.level;
            return true;
        }
    }
    return false;
}


//
// REGION maintenance
//
int Measurement::init(const std::string& region, Configuration* configuration, Component* component) {
    if ( factory.has(region) ) {
        logprintf("Region %s already exists.", region.c_str());
        return MEASUREMENT_ERROR_REGION_EXISTS;
    }
    if ( configuration == nullptr ) {
        logprintf("No measurement configuration.");
        return MEASUREMENT_ERROR_NO_CONFIG;
    }
    if ( component == nullptr ) {
        logprintf("Measurement without topology is not implemented.");
        return MEASUREMENT_ERROR_NOT_IMPLEMENTED;
    }
    if ( !factory.create(region, configuration, component) ) {
        return MEASUREMENT_ERROR_CANNOT_CREATE;
    }
    int cpu = sched_getcpu();
    if ( cpu == -1 ) {
        logprintf("Failed to determine current CPU, initializing without current CPU");
        return factory.regions[region].init();
    }
    logprintf("Initializing region %s on CPU %d", region.c_str(), cpu);
    return factory.regions[region].init(cpu);
}
int Measurement::deinit(const std::string &region)
{
    if ( !factory.has(region) ) {
        return MEASUREMENT_ERROR_REGION_NOT_EXIST;
    }
    int rv = factory.regions[region].deinit();
    factory.regions.erase(region);
    return rv;
}

//
// REGION high level API
// - implemented using low level methods
//
int Measurement::begin(const std::string& region, Configuration* configuration, Component* component) {
    int rv = init(region, configuration, component);
    if ( rv != STATUS_OK ) return rv;
    return start(region);
}
int Measurement::begin(const std::string& region, Configuration* configuration) {
    return begin(region, configuration, nullptr);
}
int Measurement::begin(const std::string& region, Component* component) {
    Configuration configuration = Configuration::fromEnvironment();
    return begin(region, &configuration, component);
}
int Measurement::begin(const std::string& region) {
    Configuration configuration = Configuration::fromEnvironment();
    return begin(region, &configuration, nullptr);
}
int Measurement::end(const std::string& region) {
    int rv = stop(region);
    if ( rv == STATUS_OK ) {
        rv = save(region);
    }
    return rv;
}

//
// REGION low level API
//
int Measurement::start(const std::string &region) {
    if ( !factory.has(region) ) {
        return MEASUREMENT_ERROR_REGION_NOT_EXIST;
    }
    return factory.regions[region].start();
}
int Measurement::read(const std::string& region) {
    if ( !factory.has(region) ) {
        return MEASUREMENT_ERROR_REGION_NOT_EXIST;
    }
    return factory.regions[region].read();
}
int Measurement::stop(const std::string& region) {
    if ( !factory.has(region) ) {
        return MEASUREMENT_ERROR_REGION_NOT_EXIST;
    }
    return factory.regions[region].stop();
}
int Measurement::save(const std::string& region) {
    if ( !factory.has(region) ) {
        return MEASUREMENT_ERROR_REGION_NOT_EXIST;
    }
    return factory.regions[region].save();
}


//
// Configuration
//
static void parseName(const std::string& name, std::function<void(const std::string&, const std::string&)> callback) {
    std::regex rgx_opt("^([^\\[]+?)\\s*\\[([^\\]]+)\\s*]$");
    std::smatch m;
    std::string no_option;
    if ( std::regex_match(name, m, rgx_opt) ) {
        callback(m[1], m[2]);
    } else {
        callback(name, no_option);
    }
}

static void splitCsv(const std::string str, std::function<void(const std::string&)> callback) {
    std::regex rgx_split("\\s*,\\s*");
    
    std::sregex_token_iterator end;
    std::string no_option;
    for (std::sregex_token_iterator iter(str.begin(), str.end(), rgx_split, -1) ; iter != end; ++iter) {
        callback(*iter);
    }
}


Measurement::Configuration::Configuration() {
}

Measurement::Configuration::Configuration(const std::vector<std::string>& eventList) {
    for(auto& e: eventList) {
        parseName(e, [&](const std::string& n, const std::string& o) {
            add(n, o);
        });
    }
}

bool Measurement::Configuration::add(const std::string& event) {
    events.emplace_back(event);
    //TODO validate name
    return true;
}
bool Measurement::Configuration::add(const std::string& event, const std::string& option) {
    events.emplace_back(event, option);
    //TODO validate name
    return true;
}

Measurement::Configuration Measurement::Configuration::fromEnvironment() {
    Measurement::Configuration config;
    char* eventList = getenv(envMetricsConfigKey);
    if ( eventList != nullptr ) {
        splitCsv(eventList, [&](const std::string& s) {
            parseName(s, [&](const std::string& n, const std::string& o) {
                config.add(n, o);
            });
        });
    }
    return config;
}
Measurement::Configuration Measurement::Configuration::fromCommandline(int argc, char* argv[]) {
    return Measurement::Configuration{};
}
Measurement::Configuration Measurement::Configuration::fromFile(const std::string& configFileName) {
    return Measurement::Configuration{};
}


int Measurement::attribHandler(std::string key, void* value, std::string* ret_value_str) {
    return 0;
}

int Measurement::attribXmlHandler(std::string key, void* value, xmlNodePtr n) {
    return EventSetManager::attribXmlHandler(key, value, n);
}

