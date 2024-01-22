#include "defines.hpp"

#ifdef PAPI_METRICS
#include "Topology.hpp"
#include "papi/Logging.hpp"

#include <vector>
#include <map>
#include <regex>

#include <papi.h>

static const char* attribMetrics = "papiMetrics";
static const char* envMetricsConfigKey = "SYS_SAGE_METRICS";

struct MetricsReading {
    long long timestamp;
    std::vector<long long> counters;
};

class MetricsStorage {
public:
    long long startTimeStamp = 0;
    std::vector<std::string> events;
    std::vector<MetricsReading> readings;

    void beginMeasurement() {
        this->startTimeStamp = PAPI_get_real_usec();
    }
    void resetMeasurement() {
        events.clear();
        readings.clear();
    }
    void addEvent(const std::string& name) {
        events.emplace_back(name);
    }
    void addCounters(const std::vector<long long>& counters) {
        readings.emplace_back(PAPI_get_real_usec(), counters);
    }
};

static MetricsStorage* getMetricsStorage(Component* component) {
    if ( component->attrib.count(attribMetrics) == 0 ) {
        component->attrib[attribMetrics] = (void*)new MetricsStorage();
    }
    return (MetricsStorage*)component->attrib[attribMetrics];
}

typedef int(*PapiCounterFunctionPtr)(int, long long*);

template<PapiCounterFunctionPtr papi_fn>
static int storeWithPapiFunction(int eventSet, Component* component) {
    int rv;
    rv = PAPI_num_events(eventSet);
    if ( rv < PAPI_OK ) {
        logprintf("Failed to get event count of eventset, err: %d", rv);
        return rv;
    }

    int num_counters = rv;
    std::vector<long long> counters(num_counters, 0);
    rv = papi_fn(eventSet, counters.data());
    if ( rv != PAPI_OK ) {
        logprintf("Failed to stop eventset, err: %d", rv);
        return rv;
    }

    // store the counters
    MetricsStorage* storage = getMetricsStorage(component);
    storage->addCounters(counters);
    return PAPI_OK;
}

static void splitCsv(const std::string str, std::function<void(const std::string&)> callback) {
    std::regex rgx_split("\\s*,\\s*");
    
    std::sregex_token_iterator end;
    std::string no_option;
    for (std::sregex_token_iterator iter(str.begin(), str.end(), rgx_split, -1) ; iter != end; ++iter) {
        callback(*iter);
    }
}


int Component::PAPI_initializeStorage(int eventSet) {
    int rv;
    rv = PAPI_num_events(eventSet);
    if ( rv < PAPI_OK ) {
        logprintf("Failed to get event count of eventset, err: %d", rv);
        return rv;
    }
    int num_events = rv;
    if ( num_events == 0 ) {
        logprintf("This eventset contains no events!");
        return PAPI_EINVAL;
    }

    int events[num_events];
    rv = PAPI_list_events(eventSet, events, &num_events);
    if ( rv != PAPI_OK ) {
        logprintf("Failed to list events of eventset, err: %d", rv);
        return rv;
    }

    MetricsStorage* storage = getMetricsStorage(this);
    storage->resetMeasurement();

    for(int event: events) {
        char name[PAPI_MAX_STR_LEN];
        rv = PAPI_event_code_to_name(event, name);
        if ( rv != PAPI_OK ) {
            logprintf("Failed to get name for event %d of eventset, err: %d", event, rv);
            return rv;
        }
        storage->addEvent(name);
    }
    
    storage->beginMeasurement();
    return PAPI_OK;
}

int Component::PAPI_stop(int eventSet) {
    return storeWithPapiFunction<::PAPI_stop>(eventSet, this);
}

int Component::PAPI_read(int eventSet) {
    return storeWithPapiFunction<::PAPI_read>(eventSet, this);
}

int Component::PAPI_accum(int eventSet) {
    return storeWithPapiFunction<::PAPI_accum>(eventSet, this);
}

bool Component::PAPI_storageInitialized() {
    return attrib.count(attribMetrics) > 0;
}

int Component::PAPI_lastCounters(std::vector<long long>& counters) {
    if ( attrib.count(attribMetrics) == 0 ) {
        logprintf("No attribute %s for component", attribMetrics);
        return PAPI_EINVAL;
    }
    MetricsStorage* storage = (MetricsStorage*)attrib[attribMetrics];
    auto num_readings = storage->readings.size();
    if ( num_readings == 0 ) {
        logprintf("No counters recorded for component!");
        return PAPI_EINVAL;
    }
    auto& lastReading = storage->readings[num_readings-1];
    counters.insert(counters.end(), lastReading.counters.begin(), lastReading.counters.end());
    return PAPI_OK;
}

std::vector<std::vector<long long>> Component::PAPI_getCounters() {
    std::vector<std::vector<long long>> countersList;
    if ( attrib.count(attribMetrics) > 0 ) {
        MetricsStorage* storage = (MetricsStorage*)attrib[attribMetrics];
        for(auto& reading: storage->readings) {
            countersList.push_back(reading.counters);
        }
    }
    return countersList;
}

int Component::PAPI_attribHandler(std::string key, void* value, std::string* ret_value_str) {
    return 0;
}

int Component::PAPI_attribXmlHandler(std::string key, void* value, xmlNodePtr n) {
    if(key.compare(attribMetrics) == 0) {
        MetricsStorage* storage = (MetricsStorage*)value;
        auto num_events = storage->events.size();       
        if ( num_events == 0 || storage->startTimeStamp == 0 ) {
            logprintf("No registered events found, not generating XML.");
            return 0;
        }

        xmlNodePtr attrib_node = xmlNewNode(NULL, (const unsigned char *)"Attribute");
        xmlNewProp(attrib_node, (const unsigned char *)"name", (const unsigned char *)"PapiMetrics");
        xmlAddChild(n, attrib_node);
        for(size_t i=0; i<num_events; i++) {
            xmlNodePtr eventNode = xmlNewNode(NULL, (const unsigned char *)"Event");
            xmlNewProp(eventNode, (const unsigned char *)"name", (const unsigned char *)storage->events[i].c_str());
            xmlAddChild(attrib_node, eventNode);

            for(auto& reading: storage->readings) {
                xmlNodePtr counterNode = xmlNewNode(NULL, (const unsigned char *)"Counter");

                auto elapsed = std::to_string(reading.timestamp - storage->startTimeStamp);
                auto counter = std::to_string(reading.counters[i]);
                xmlNewProp(counterNode, (const unsigned char *)"elapsed", (const unsigned char *)elapsed.c_str());
                xmlNewProp(counterNode, (const unsigned char *)"value", (const unsigned char *)counter.c_str());
                xmlAddChild(eventNode, counterNode);
            }

        }
        return 1;
    }
    return 0;
}

std::vector<std::string> Component::PAPI_EventsFromEnvironment() {
    std::vector<std::string> events;
    char* eventList = getenv(envMetricsConfigKey);
    if ( eventList != nullptr ) {
        splitCsv(eventList, [&](const std::string& s) {
            events.push_back(s);
        });
    }
    return events;
}
#endif