#include "papi/Metrics.hpp"
#include "papi/Logging.hpp"
#include "papi/Storage.hpp"
#include "papi/Binding.hpp"
#include "papi/Utility.hpp"

#include "xml_dump.hpp"

#include <papi.h>

#include <map>
#include <regex>
#include <numeric>

using namespace papi;

static const char* envMetricsConfigKey = "SYS_SAGE_METRICS";
static StorageManager storageManager;

int SYSSAGE_PAPI_start(int eventSet) {
    return storageManager.start(eventSet);
}

int SYSSAGE_PAPI_accum(int eventSet) {
    return storageManager.accum(eventSet);
}

int SYSSAGE_PAPI_read(int eventSet) {
    return storageManager.read(eventSet);
}

int SYSSAGE_PAPI_stop(int eventSet) {
    return storageManager.stop(eventSet);
}

int SYSSAGE_PAPI_destroy_eventset(int* eventSet) {
    if ( eventSet == nullptr ) return PAPI_EINVAL;
    int rv = storageManager.destroy(*eventSet);
    *eventSet = PAPI_NULL;
    return rv;
}

int SYSSAGE_PAPI_stop(int eventSet, Component* component) {
    int rv = SYSSAGE_PAPI_stop(eventSet);
    if ( rv != PAPI_OK ) return rv;

    return SYSSAGE_PAPI_bind(eventSet, component);
}

int SYSSAGE_PAPI_stop(int eventSet, Topology* topology, Component** boundComponent) {
    int rv = SYSSAGE_PAPI_stop(eventSet);
    if ( rv != PAPI_OK ) return rv;

    return SYSSAGE_PAPI_automatic_bind(eventSet, topology, boundComponent);
}

int SYSSAGE_PAPI_stop(int eventSet, Topology* topology, std::vector<Component*>& boundComponents) {
    int rv = SYSSAGE_PAPI_stop(eventSet);
    if ( rv != PAPI_OK ) return rv;

    return SYSSAGE_PAPI_automatic_bind(eventSet, topology, boundComponents);
}

int SYSSAGE_PAPI_print(int eventSet) {
    papi::Printer printer;
    return storageManager.data(eventSet, printer);
}

int SYSSAGE_PAPI_visit(int eventSet, SYSSAGE_PAPI_Visitor& visitor) {
    return storageManager.data(eventSet, visitor);
}

int SYSSAGE_PAPI_add_values(int eventSet, const std::vector<long long>& values) {
    return storageManager.store(eventSet, values);
}

// low level/internal methods for connecting an eventset to a component
int SYSSAGE_PAPI_bind(int eventSet, Component* component, const std::vector<int>& indices) {
    if ( storageManager.getStorage(eventSet) == nullptr ) {
        logprintf("Eventset not stored, cannot bind to component");
        return PAPI_ENOEVST;
    }
    if ( component == nullptr ) {
        logprintf("Component value cannot be null");
        return PAPI_EINVAL;
    }
    PapiMetricsAttrib* papiAttrib = PapiMetricsAttrib::getMetricsAttrib(component);
    auto& vec = papiAttrib->eventSets;
    EventSetSubSet subSet{eventSet, indices};
    if ( std::find(vec.begin(), vec.end(), subSet) == vec.end() ) {
        vec.push_back(subSet);
    } else {
        logprintf("Eventset %d has already been added to component", eventSet);
        // not an error, simply ignore
    }
    return PAPI_OK;
}

int SYSSAGE_PAPI_bind(int eventSet, Component* component) {
    return SYSSAGE_PAPI_bind(eventSet, component, {});
}

int SYSSAGE_PAPI_unbind(int eventSet, Component* component) {
    if ( component == nullptr ) {
        logprintf("Component value cannot be null");
        return PAPI_EINVAL;
    }
    PapiMetricsAttrib* papiAttrib = PapiMetricsAttrib::getMetricsAttrib(component);
    auto& vec = papiAttrib->eventSets;
    vec.erase(
        std::remove_if(
            vec.begin(), 
            vec.end(),
            [eventSet](const EventSetSubSet& es) { return es.eventSet == eventSet; }
        ), 
        vec.end()); 

    return PAPI_OK;
}

int SYSSAGE_PAPI_automatic_bind(int eventSet, Topology* topology, Component** boundComponent) {
    auto storage = storageManager.getStorage(eventSet);
    if ( storage == nullptr ) {
        logprintf("Eventset not stored, cannot bind to component");
        return PAPI_ENOEVST;
    }
    if ( topology == nullptr ) {
        logprintf("Component value cannot be null");
        return PAPI_EINVAL;
    }

    // trying to determine dominant core/hw thread of the eventset
    int core = storage->eventSetInfo.core;
    int type = SYS_SAGE_COMPONENT_THREAD;
    if ( core == -1 ) {
        // the eventset was not attached to a cpu, search for core values in readings
        auto stat = storageManager.cpuStat(eventSet);
        if ( !stat.empty() ) {
            auto max_pair = std::max_element(stat.begin(), stat.end(), [](auto& a, auto& b) {
                return a.second < b.second;
            });
            core = max_pair->first;
        } else {
            // use start core of the first session
            if ( storage->sessions.size() > 0 ) {
                core = storage->sessions[0].startCore;
            }
        }
    }

    Component* component;
    if ( core != -1 ) {
        logprintf("Automatic binding eventset %d to THREAD component with ID %d", eventSet, core);
        component = topology->FindSubcomponentById(core, type);
    }
    if ( core == -1 || component == nullptr ) {
        type = SYS_SAGE_COMPONENT_NODE;
        std::vector<Component*> nodes;
        topology->FindAllSubcomponentsByType(&nodes, type);
        if ( nodes.size() > 0 ) {
            logprintf("Automatic binding eventset %d to node, because no core found", eventSet);
            component = nodes[0];
        } else {
            // fallback
            logprintf("Automatic binding eventset %d to fallback to topology", eventSet);
            component = topology;
        }
    }
    if ( boundComponent != nullptr ) *boundComponent = component;
    return SYSSAGE_PAPI_bind(eventSet, component);
}

// scattering eventset results between components
int SYSSAGE_PAPI_automatic_bind(int eventSet, Topology* topology, std::vector<Component*>& boundComponents) {
    Component* automaticComponent;
    int rv = SYSSAGE_PAPI_automatic_bind(eventSet, topology, &automaticComponent);
    if ( rv != PAPI_OK ) {
        return rv;
    }
    if ( automaticComponent->GetComponentType() == SYS_SAGE_COMPONENT_NODE ) {
        // the automatic bind could not determine core
        boundComponents.push_back(automaticComponent);
        return rv;
    }

    auto storage = storageManager.getStorage(eventSet);
    int num_events = (int)storage->eventSetInfo.events.size();
    Thread* hwThreadComponent = (Thread*)automaticComponent;
    auto cpuId = hwThreadComponent->GetId();
    SYSSAGE_PAPI_unbind(eventSet, automaticComponent);

    std::map<Component*, std::vector<int>> mapping;
    for(int index = 0; index < num_events; index++) {
        auto& eventName = storage->eventSetInfo.events[index];
        auto info = ComponentInfo::getComponentForEvent(eventName);
        logprintf("Recommended component type for %s is %d (%d)", eventName.c_str(), info.componentType, info.level);
        // find components by type
        std::vector<Component*> components;
        topology->FindAllSubcomponentsByType(&components, info.componentType);
        Component* foundComponent = nullptr;
        for(auto component: components) {
            // special case, for cache components the level must be checked
            if ( info.componentType == SYS_SAGE_COMPONENT_CACHE ) {
                Cache* cache = (Cache*)component;
                if ( cache->GetCacheLevel() != info.level ) continue;
            }
            if ( ComponentInfo::component_has_hwthread(component, cpuId) ) {
                foundComponent = component;
                break;
            }
        }
        if ( foundComponent != nullptr ) {
            if ( mapping.count(foundComponent) == 0 ) {
                mapping[foundComponent] = {};
            }
            mapping[foundComponent].push_back(index);
            logprintf("Adding event %s with index %d to component %s", eventName.c_str(), index, foundComponent->GetName().c_str());
        } else {
            logprintf("No component found for event %s", eventName.c_str());
        }
    }
    for(auto& [component, list]: mapping) {
        boundComponents.push_back(component);
        int rv = SYSSAGE_PAPI_bind(eventSet, component, list);
        if ( rv != PAPI_OK ) return rv;
    }
    return PAPI_OK;
}

static int PAPI_MetricsXmlHandler(PapiMetricsAttrib* metricsAttrib, xmlNodePtr n) {
    xmlNodePtr attrib_node = xmlNewNode(NULL, (const unsigned char *)"Attribute");
    xmlNewProp(attrib_node, (const unsigned char *)"name", (const unsigned char *)"PapiMetrics");
    xmlAddChild(n, attrib_node);

    for(auto& eventSubSet: metricsAttrib->eventSets) {
        auto storage = storageManager.getStorage(eventSubSet.eventSet);
        if ( storage == nullptr ) {
            logprintf("Cannot XML export eventset %d, storage not found", eventSubSet.eventSet);
            continue;
        }

        std::vector<int> eventIndices{eventSubSet.eventIndices};
        if ( eventIndices.size() == 0 )  {
            eventIndices.resize(storage->eventSetInfo.events.size());
            std::iota(eventIndices.begin(), eventIndices.end(), 0);
        }

        xmlNodePtr eventSetNode = xmlNewNode(NULL, (const unsigned char *)"EventSet");
        auto idStr = std::to_string(eventSubSet.eventSet);
        xmlNewProp(eventSetNode, (const unsigned char *)"id", (const unsigned char *)idStr.c_str());
        if ( storage->eventSetInfo.core != -1 ) {
            auto coreStr = std::to_string(storage->eventSetInfo.core);
            xmlNewProp(eventSetNode, (const unsigned char *)"core", (const unsigned char *)coreStr.c_str());
        }
        if ( storage->eventSetInfo.tid != 0 ) {
            auto tidStr = std::to_string(storage->eventSetInfo.tid);
            xmlNewProp(eventSetNode, (const unsigned char *)"tid", (const unsigned char *)tidStr.c_str());
        }
        xmlAddChild(attrib_node, eventSetNode);

        for(auto& session: storage->sessions) {
            xmlNodePtr measurementNode = xmlNewNode(NULL, (const unsigned char *)"Measurement");
            auto startStr = std::to_string(session.startTimeStamp);
            xmlNewProp(measurementNode, (const unsigned char *)"startTimestamp", (const unsigned char *)startStr.c_str());
            if ( session.stopTimeStamp ) {
                auto stopStr = std::to_string(session.stopTimeStamp);
                xmlNewProp(measurementNode, (const unsigned char *)"stopTimestamp", (const unsigned char *)stopStr.c_str());
            }
            xmlAddChild(eventSetNode, measurementNode);
            for(auto index: eventIndices) {
                xmlNodePtr eventNode = xmlNewNode(NULL, (const unsigned char *)"Event");
                auto& eventName = storage->eventSetInfo.events[index];
                xmlNewProp(eventNode, (const unsigned char *)"name", (const unsigned char *)eventName.c_str());
                xmlAddChild(measurementNode, eventNode);

                for(auto& reading: session.readings) {
                    xmlNodePtr counterNode = xmlNewNode(NULL, (const unsigned char *)"Counter");

                    auto timestampStr = std::to_string(reading.timestamp);
                    //FIXME use elapsed instead of timestamp?
                    auto counterStr = std::to_string(reading.counters[index]);
                    xmlNewProp(counterNode, (const unsigned char *)"timestamp", (const unsigned char *)timestampStr.c_str());
                    xmlNewProp(counterNode, (const unsigned char *)"value", (const unsigned char *)counterStr.c_str());
                    xmlAddChild(eventNode, counterNode);
                }
            }
        }
    }
    return 1;
}

static int PAPI_MetricsXmlHandler(PapiMetricsTable* metricsTable, xmlNodePtr n) {
    xmlNodePtr attrib_node = xmlNewNode(NULL, (const unsigned char *)"Attribute");
    xmlNewProp(attrib_node, (const unsigned char *)"name", (const unsigned char *)"PapiMetricsTable");
    xmlAddChild(n, attrib_node);
    int tables = 0;
    for(auto& table: metricsTable->tables) {
        xmlNodePtr tableNode = xmlNewNode(NULL, (const unsigned char *)"Table");
        xmlAddChild(attrib_node, tableNode);

        xmlNodePtr headerNode = xmlNewNode(NULL, (const unsigned char *)"Header");
        xmlAddChild(tableNode, headerNode);

        for(auto& column: table.headers) {
            xmlNodePtr columnNode = xmlNewNode(NULL, (const unsigned char *)"Column");
            xmlNodePtr columnName = xmlNewText((const unsigned char *)column.c_str());
            xmlAddChild(columnNode, columnName);
            xmlAddChild(headerNode, columnNode);
        }
        for(auto& row: table.rows) {
            xmlNodePtr rowNode = xmlNewNode(NULL, (const unsigned char *)"Row");
            for(auto& value: row) {
                xmlNodePtr valueNode = xmlNewNode(NULL, (const unsigned char *)"Value");
                xmlNodePtr valueName = xmlNewText((const unsigned char *)value.c_str());
                xmlAddChild(valueNode, valueName);
                xmlAddChild(rowNode, valueNode);
            }
            xmlAddChild(tableNode, rowNode);
        }
        tables++;
    }
    return tables > 0 ? 1 : 0;
}

/// @brief Metrics attribute handler
static int PAPI_attribHandler(std::string key, void* value, std::string* ret_value_str) {
    // serializing PapiMetricsAttrib and PapiMetricsTable to string is not supported
    return 0;
}

/// @brief Metrics attribute handler for XML export
static int PAPI_attribXmlHandler(std::string key, void* value, xmlNodePtr n) {
    if(key.compare(PapiMetricsAttrib::attribMetrics) == 0) {
        PapiMetricsAttrib* metricsAttrib = (PapiMetricsAttrib*)value;
        if ( metricsAttrib->eventSets.size() == 0 ) {
            return 0;
        }
        return PAPI_MetricsXmlHandler(metricsAttrib, n);
    } else if(key.compare(PapiMetricsTable::attribMetricsTable) == 0) {
        PapiMetricsTable* metricsTable = (PapiMetricsTable*)value;
        if ( metricsTable->tables.size() == 0 ) {
            return 0;
        }
        return PAPI_MetricsXmlHandler(metricsTable, n);
    }

    return 0;
}

int SYSSAGE_PAPI_export_xml(Component* topology, const std::string& path) {
    return exportToXml(topology, path, PAPI_attribHandler, PAPI_attribXmlHandler);
}


struct SubsetFilter : public SYSSAGE_PAPI_Visitor {
    SYSSAGE_PAPI_Visitor& forward;
    std::vector<int> eventIndices;
    SubsetFilter(SYSSAGE_PAPI_Visitor& fwd, std::vector<int> indices) : forward{fwd}, eventIndices{indices} {}

    bool data(int sid, long long sts, long long ts, int core, const std::vector<long long>& counters) {
        std::vector<long long> selectedCounters;
        std::vector<std::string> names;
        for(auto index: eventIndices) {
            selectedCounters.push_back(counters[index]);
        }
        return forward.data(sid, sts, ts, core, selectedCounters);
    }

    void info(int eventSet, int core, unsigned long tid, const std::vector<std::string>& eventNames) {
        std::vector<std::string> names;
        for(auto index: eventIndices) {
            names.push_back(eventNames[index]);
        }
        return forward.info(eventSet, core, tid, names);
    }    
};

int SYSSAGE_PAPI_freeze(Component* component, SYSSAGE_PAPI_Freezer& freezer) {
    for(Component* child: *component->GetChildren()) {
        if ( PapiMetricsAttrib::existsMetricsAttrib(child) ) {
            logprintf("Component has metricsattrib");
            auto metricsAttrib = PapiMetricsAttrib::getMetricsAttrib(child);
            for(auto& eventSet: metricsAttrib->eventSets) {
                // cleanup freezer
                freezer.defrost();
                // build data table
                if ( eventSet.eventIndices.size() > 0 ) {
                    logprintf("Filtering and freezing %d", eventSet.eventSet);
                    SubsetFilter filter{freezer, eventSet.eventIndices};
                    storageManager.data(eventSet.eventSet, filter);
                } else {
                    logprintf("Freezing %d", eventSet.eventSet);
                    storageManager.data(eventSet.eventSet, freezer);
                }
                auto metricsTable = PapiMetricsTable::getMetricsTable(child);
                metricsTable->tables.push_back(freezer.frozen());
            }
            PapiMetricsAttrib::deleteMetricsAttrib(child);
        }
        SYSSAGE_PAPI_freeze(child, freezer);
    }

    return PAPI_OK;
}

int SYSSAGE_PAPI_freeze(Component* component) {
    papi::DefaultFreezer freezer;
    return SYSSAGE_PAPI_freeze(component, freezer);
}

int SYSSAGE_PAPI_cleanup(Component* component) {
    for(Component* child: *component->GetChildren()) {
        PapiMetricsAttrib::deleteMetricsAttrib(child);
        PapiMetricsTable::deleteMetricsTable(child);
        SYSSAGE_PAPI_cleanup(child);
    }

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

/// @brief Retrieves the vector of PAPI event names from the environment variable SYS_SAGE_METRICS
std::vector<std::string> SYSSAGE_PAPI_EventsFromEnvironment() {
    std::vector<std::string> events;
    char* eventList = getenv(envMetricsConfigKey);
    if ( eventList != nullptr ) {
        splitCsv(eventList, [&](const std::string& s) {
            events.push_back(s);
        });
    }
    return events;
}

