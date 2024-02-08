#include "papi/Metrics.hpp"
#include "papi/Logging.hpp"
#include "papi/SystemInfo.hpp"
#include "xml_dump.hpp"

#include <papi.h>

#include <map>
#include <utility>
#include <regex>
#include <iomanip>
#include <numeric>

static const char* attribMetrics = "papiMetrics";
static const char* attribMetricsTable = "papiMetricsTable";
static const char* envMetricsConfigKey = "SYS_SAGE_METRICS";

#define TIMESTAMP_FUNCTION PAPI_get_real_usec

struct ComponentInfo {
    int componentType;
    // additional attributes
    int level = 0;
    ComponentInfo(int t, int l = 0) : componentType{t}, level{l} {}

    static bool component_has_hwthread(Component* component, int hwThreadId) {
        Thread * t = (Thread*)component->FindSubcomponentById(hwThreadId, SYS_SAGE_COMPONENT_THREAD);
        return t != nullptr;
    }

    static ComponentInfo getComponentForEvent(const std::string& eventName) {
        std::map<std::string, ComponentInfo> rules = {
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
};

struct EventSetInfo {
    int eventSet;
    std::vector<std::string> events;
    unsigned long tid;
    int core;

    EventSetInfo(int es, const std::vector<std::string>& e, unsigned long t = 0, int cpu = -1)
    : eventSet{es}, events{e}, tid{t}, core{cpu}
    {}
    
    EventSetInfo()
    : EventSetInfo{0, {}}
    {}

    EventSetInfo(const EventSetInfo& copy)
    : EventSetInfo(copy.eventSet, copy.events, copy.tid, copy.core)
    {}

    bool operator==(const EventSetInfo& other) const {
		return eventSet == other.eventSet
            && events == other.events
            && tid == other.tid
            && core == other.core;
    }

	bool operator()(const EventSetInfo& a, const EventSetInfo& b) const {
        return a == b;
	}

    static std::pair<int, EventSetInfo> createEventSetInfo(int eventSet) {
        int rv;
        int state;
        unsigned long tid = 0;
        int core = -1;

        rv = PAPI_state(eventSet, &state);
        if ( rv < PAPI_OK ) {
            logprintf("Failed to get state of eventset, err: %d", rv);
            return std::make_pair(rv, EventSetInfo{});
        }

        if ( (state & PAPI_NOT_INIT) == PAPI_NOT_INIT ) {
            logprintf("Eventset is not initialized");
            return std::make_pair(PAPI_ENOINIT, EventSetInfo{});
        }

        rv = PAPI_num_events(eventSet);
        if ( rv < PAPI_OK ) {
            logprintf("Failed to get event count of eventset, err: %d", rv);
            return std::make_pair(rv, EventSetInfo{});
        }

        int num_events = rv;
        int events[num_events];
        rv = PAPI_list_events(eventSet, events, &num_events);
        if ( rv != PAPI_OK ) {
            logprintf("Failed to list events of eventset, err: %d", rv);
            return std::make_pair(rv, EventSetInfo{});
        }

        std::vector<std::string> eventNames;
        for(int event: events) {
            char name[PAPI_MAX_STR_LEN];
            rv = PAPI_event_code_to_name(event, name);
            if ( rv != PAPI_OK ) {
                logprintf("Failed to get name for event %d of eventset, err: %d", event, rv);
                return std::make_pair(rv, EventSetInfo{});
            }
            eventNames.push_back(name);
        }
        
        if ( (state & PAPI_ATTACHED) == PAPI_ATTACHED ) {
            PAPI_option_t attach_option;
            attach_option.attach.eventset = eventSet;
            attach_option.attach.tid = PAPI_NULL;
            rv = PAPI_get_opt(PAPI_ATTACH, &attach_option);
            if ( rv < PAPI_OK ) {
                logprintf("Failed to get attach option of eventset, err: %d", rv);
                return std::make_pair(rv, EventSetInfo{});
            }
            tid = attach_option.attach.tid;
        }

        if ( (state & PAPI_CPU_ATTACHED) == PAPI_CPU_ATTACHED ) {
            PAPI_option_t cpu_option;
            cpu_option.cpu.eventset = eventSet;
            cpu_option.cpu.cpu_num = PAPI_NULL;
            rv = PAPI_get_opt(PAPI_CPU_ATTACH, &cpu_option);
            if ( rv < PAPI_OK ) {
                logprintf("Failed to get cpu attach option of eventset, err: %d", rv);
                return std::make_pair(rv, EventSetInfo{});
            }
            core = cpu_option.cpu.cpu_num;
        }

        return std::make_pair(PAPI_OK, EventSetInfo{eventSet, eventNames, tid, core});
    }
};

struct EventSetReading {
    long long timestamp;
    int core;
    std::vector<long long> counters;

    EventSetReading(long long ts, int c, const std::vector<long long>& values)
    : timestamp{ts}, core{c}, counters{values} 
    {}
};

struct EventSetStorageSession {
    long long startTimeStamp = 0;
    int startCore = -1;
    long long stopTimeStamp = 0;
    int stopCore = -1;
    std::vector<EventSetReading> readings;

    EventSetStorageSession(long long ts, int c) : startTimeStamp{ts}, startCore{c} {}

    int stop(long long ts, int core) {
        if ( startTimeStamp == 0 ) {
            return PAPI_ENOTRUN;
        }
        if ( stopTimeStamp != 0 ) {
            return PAPI_ENOTRUN;
        }
        stopTimeStamp = ts;
        stopCore = core;
        return PAPI_OK;
    }
    int add(long long ts, int core, const std::vector<long long>& counters) {
        if ( startTimeStamp == 0 ) {
            return PAPI_ENOTRUN;
        }
        if ( stopTimeStamp != 0 ) {
            return PAPI_ENOTRUN;
        }
        readings.emplace_back(ts, core, counters);
        return PAPI_OK;
    }
};

struct EventSetStorage {
    EventSetInfo eventSetInfo;
    std::vector<EventSetStorageSession> sessions;

    EventSetStorage(const EventSetInfo& info) : eventSetInfo{info} {}

    EventSetStorage() {
        logprintf("WARNING: uninitialized EventSetStorage is used!");
    }
    //EventSetStorage(const EventSetStorage& copy) = delete;

    int addCounters(const std::vector<long long>& counters) {
        auto num_sessions = sessions.size();
        if ( num_sessions == 0 ) {
            logprintf("No storage session found!");
            return PAPI_ENOINIT;
        }
        int core = eventSetInfo.core != -1 ? eventSetInfo.core : papi::SystemInfo::getThreadCpu(eventSetInfo.tid);
        return sessions[num_sessions-1].add(TIMESTAMP_FUNCTION(), core, counters);
    }

    int start() {
        int core = eventSetInfo.core != -1 ? eventSetInfo.core : papi::SystemInfo::getThreadCpu(eventSetInfo.tid);
        sessions.emplace_back(TIMESTAMP_FUNCTION(), core);
        return PAPI_OK;
    }

    int stop() {
        auto num_sessions = sessions.size();
        if ( num_sessions == 0 ) {
            logprintf("No storage session found!");
            return PAPI_ENOINIT;
        }
        int core = eventSetInfo.core != -1 ? eventSetInfo.core : papi::SystemInfo::getThreadCpu(eventSetInfo.tid);
        sessions[num_sessions-1].stop(TIMESTAMP_FUNCTION(), core);
        return PAPI_OK;
    }
};

// manages storage for eventsets
class StorageManager {
    //std::map<EventSetInfo, EventSetStorage, EventSetInfo> eventSets;
    std::map<int, EventSetStorage> eventSetStorage;

public:
    EventSetStorage* getStorage(int eventSet) {
        if ( eventSetStorage.count(eventSet) > 0 ) {
            return &eventSetStorage[eventSet];
        }
        return nullptr;
    }

    int start(int eventSet) {
        auto check = EventSetInfo::createEventSetInfo(eventSet);
        if ( check.first != PAPI_OK ) return check.first;

        // check cache
        bool should_create = false;
        if ( eventSetStorage.count(eventSet) > 0 ) {
            if ( eventSetStorage[eventSet].eventSetInfo != check.second ) {
                logprintf("EventSet exists but has been changed, cannot start");
                return SYSSAGE_PAPI_ECHANGED;
            }
        } else {
            should_create = true;
        }

        int rv = ::PAPI_start(eventSet);
        if ( rv == PAPI_OK ) {
            if ( should_create ) {
                eventSetStorage.emplace(eventSet, EventSetStorage{check.second});
            }
            eventSetStorage[eventSet].start();
        }
        return rv;
    }

    typedef int(*PapiCounterFunctionPtr)(int, long long*);
    //using PapiCounterFunctionPtr = std::function<int(int, long long*)>;
    template<PapiCounterFunctionPtr papi_fn>
    int store(int eventSet) {
        int rv;

        if ( eventSetStorage.count(eventSet) == 0 ) {
            logprintf("Cannot save counters, storage for EventSet %d not yet initialized", eventSet);
            return PAPI_ENOINIT;
        }

        //FIXME: the eventset cannot be modified once started -> check!
        auto& info = eventSetStorage[eventSet].eventSetInfo;
        auto check = EventSetInfo::createEventSetInfo(eventSet);
        if ( info != check.second ) {
            logprintf("EventSet has been changed, ignoring new counters");
            return SYSSAGE_PAPI_ECHANGED;
        }

        int num_counters = info.events.size();
        std::vector<long long> counters(num_counters, 0);
        rv = papi_fn(eventSet, counters.data());
        if ( rv != PAPI_OK ) {
            logprintf("Failed to stop|read|accum eventset, err: %d", rv);
            return rv;
        }

        return eventSetStorage[eventSet].addCounters(counters);
    }

    int store(int eventSet, const std::vector<long long>& values) {
        if ( eventSetStorage.count(eventSet) == 0 ) {
            logprintf("Cannot save counters, storage for EventSet %d not yet initialized", eventSet);
            return PAPI_ENOINIT;
        }
        auto& info = eventSetStorage[eventSet].eventSetInfo;
        auto num_counters = info.events.size();
        if ( num_counters != values.size() ) {
            logprintf("Cannot save values, the number of values are different, expected: %ld, got: %ld", num_counters, values.size());
            return PAPI_ECNFLCT;
        }

        return eventSetStorage[eventSet].addCounters(values);
    }

    int stop(int eventSet) {
        if ( eventSetStorage.count(eventSet) == 0 ) {
            logprintf("Cannot stop storage, storage for EventSet %d not yet initialized", eventSet);
            return PAPI_ENOINIT;
        }

        return eventSetStorage[eventSet].stop();
    }

    int destroy(int eventSet) {
        if ( eventSetStorage.count(eventSet) == 0 ) {
            logprintf("Cannot destroy storage, storage for EventSet %d not yet initialized", eventSet);
            return PAPI_ENOINIT;
        }

        // deleting storage
        eventSetStorage.erase(eventSet);

        int e = eventSet;
        return ::PAPI_destroy_eventset(&e);
    }

    int data(int eventSet, SYSSAGE_PAPI_Visitor& visitor) {
        if ( eventSetStorage.count(eventSet) == 0 ) {
            logprintf("Cannot print eventSet, storage for EventSet %d not initialized", eventSet);
            return PAPI_ENOINIT;
        }
        auto& storage = eventSetStorage[eventSet];
        visitor.info(storage.eventSetInfo.eventSet, storage.eventSetInfo.core, storage.eventSetInfo.tid, storage.eventSetInfo.events);
        int sessionId = 0;
        for(auto& session: storage.sessions) {
            for(auto& reading: session.readings) {
                bool keep_running = visitor.data(sessionId, session.startTimeStamp, reading.timestamp, reading.core, reading.counters);
                if ( !keep_running ) return PAPI_OK;
            }
            sessionId++;
        }
        return PAPI_OK;
    }

    std::map<int, int> cpuStat(int eventSet) {
        std::map<int, int> stat;
        if ( eventSetStorage.count(eventSet) > 0 ) {
            auto& storage = eventSetStorage[eventSet];
            for(auto& session: storage.sessions) {
                for(auto& reading: session.readings) {
                    if ( reading.core != -1 ) {
                        stat[reading.core]++;
                    }
                }
            }
        }
        return stat;
    }
};

struct EventSetSubSet {
    int eventSet;
    std::vector<int> eventIndices;
    EventSetSubSet(int es, const std::vector<int>& ix) : eventSet{es}, eventIndices{ix} {}
    EventSetSubSet(int es) : eventSet{es} {}

    bool operator==(const EventSetSubSet& other) const {
		return eventSet == other.eventSet
            && eventIndices == other.eventIndices;
    }
};

struct PapiMetricsAttrib {
    std::vector<EventSetSubSet> eventSets;

    static PapiMetricsAttrib* getMetricsAttrib(Component* component) {
        if ( !existsMetricsAttrib(component) ) {
            component->attrib[attribMetrics] = (void*)new PapiMetricsAttrib();
            //logprintf("Create MetricsAttrib for %s", component->GetName().c_str());
        }
        return (PapiMetricsAttrib*)component->attrib[attribMetrics];
    }
    static inline bool existsMetricsAttrib(Component* component) {
        return component->attrib.count(attribMetrics) > 0;
    }
    static inline void deleteMetricsAttrib(Component* component) {
        if ( !existsMetricsAttrib(component) ) {
            PapiMetricsAttrib* pma = getMetricsAttrib(component);
            if ( pma != nullptr ) delete pma;
            component->attrib.erase(attribMetrics);
        }
    }
};

struct PapiMetricsTable {
    std::vector<SYSSAGE_PAPI_DataTable<std::string>> tables;

    static PapiMetricsTable* getMetricsTable(Component* component) {
        if ( !existsMetricsTable(component) ) {
            component->attrib[attribMetricsTable] = (void*)new PapiMetricsTable();
            //logprintf("Create MetricsTable for %s", component->GetName().c_str());
        }
        return (PapiMetricsTable*)component->attrib[attribMetricsTable];
    }
    static inline bool existsMetricsTable(Component* component) {
        return component->attrib.count(attribMetricsTable) > 0;
    }
    static inline void deleteMetricsTable(Component* component) {
        if ( existsMetricsTable(component) ) {
            PapiMetricsTable* pmt = getMetricsTable(component);
            if ( pmt != nullptr ) delete pmt;
            component->attrib.erase(attribMetricsTable);
        }
    }
};

struct DefaultFreezer: public SYSSAGE_PAPI_Freezer<std::string> {
    SYSSAGE_PAPI_DataTable<std::string> table;

    SYSSAGE_PAPI_DataTable<std::string>& frozen() {
        return table;
    }

    void defrost() {
        table.headers.clear();
        table.rows.clear();
    }

    bool data(int sid, long long sts, long long ts, int core, const std::vector<long long>& counters) {
        auto elapsed = ts - sts;
        if ( elapsed > 0 ) {
            std::vector<std::string> columns;
            columns.push_back(std::to_string(elapsed));
            for(auto& c: counters) {
                columns.push_back(std::to_string(c));
            }
            table.rows.emplace_back(columns);
        }
        return true;
    }

    void info(int eventSet, int core, unsigned long tid, const std::vector<std::string>& eventNames) {
        table.headers.push_back("time (us)");
        for(auto& e: eventNames) {
            table.headers.push_back(e);
        }
    }
};

struct Printer : public SYSSAGE_PAPI_Visitor {
    int column_width = 16;
    int session_id = -1;

    Printer(int width = 16) : column_width{width} {}

    bool data(int sid, long long sts, long long ts, int core, const std::vector<long long>& counters) {
        if ( sid != session_id ) {
            std::cout << "Session " << sid << " start timestamp: " << sts << std::endl;
            session_id = sid;
        }
        std::cout 
            << std::setw(column_width) << ts
            << std::setw(column_width) << core;
        for(auto& c: counters) {
            std::cout << std::setw(column_width) << c;
        }
        std::cout << std::endl;
        return true;
    }

    void info(int eventSet, int core, unsigned long tid, const std::vector<std::string>& eventNames) {
        std::cout << "EventSet: " << eventSet << std::endl;
        if ( tid > 0 ) {
            std::cout << "Attached TID: " << tid << std::endl;
        }
        if ( core > -1 ) {
            std::cout << "Attached CPU: " << core << std::endl;
        }
        std::cout 
            << std::setw(column_width) << "timestamp"
            << std::setw(column_width) << "core";
        for(auto& e: eventNames) {
            std::cout << std::setw(column_width) << e;
        }
        std::cout << std::endl;
    }
};

static StorageManager storageManager;

int SYSSAGE_PAPI_start(int eventSet) {
    return storageManager.start(eventSet);
}

int SYSSAGE_PAPI_accum(int eventSet) {
    return storageManager.store<::PAPI_accum>(eventSet);
}

int SYSSAGE_PAPI_read(int eventSet) {
    return storageManager.store<::PAPI_read>(eventSet);
}

int SYSSAGE_PAPI_stop(int eventSet) {
    int rv = storageManager.store<::PAPI_stop>(eventSet);
    // stop active storage session anyway
    storageManager.stop(eventSet);
    return rv;
}

/// destroy PAPI eventset with PAPI_destroy and frees counter storage
int SYSSAGE_PAPI_destroy(int eventSet) {
    return storageManager.destroy(eventSet);
}

int SYSSAGE_PAPI_print(int eventSet) {
    Printer printer;
    return storageManager.data(eventSet, printer);
}

int SYSSAGE_PAPI_visit(int eventSet, SYSSAGE_PAPI_Visitor& visitor) {
    return storageManager.data(eventSet, visitor);
}

//
// data collection API
// b) with sys-sage Component
//
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


// low level/internal
// optional: one can add arbitrary values to be stored with papi data
// the number of values must match the eventset num_events!
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
    //TODO implement findCommonAncestor
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

/// @brief Metrics attribute handler
static int PAPI_attribHandler(std::string key, void* value, std::string* ret_value_str) {
    return 0;
}

/// @brief Metrics attribute handler for XML export
static int PAPI_attribXmlHandler(std::string key, void* value, xmlNodePtr n) {
    if(key.compare(attribMetrics) == 0) {

        PapiMetricsAttrib* metricsAttrib = (PapiMetricsAttrib*)value;
        if ( metricsAttrib->eventSets.size() == 0 ) {
            return 0;
        }

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
                for(size_t i=0; i<eventIndices.size(); i++) {
                    int index = eventIndices[i];
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
    } else if(key.compare(attribMetricsTable) == 0) {
        PapiMetricsTable* metricsTable = (PapiMetricsTable*)value;
        if ( metricsTable->tables.size() == 0 ) {
            return 0;
        }

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

    return 0;
}

int SYSSAGE_PAPI_export_xml(Component* topology, const std::string& path) {
    return exportToXml(topology, path, PAPI_attribHandler, PAPI_attribXmlHandler);
}

int SYSSAGE_PAPI_freeze(Component* component, SYSSAGE_PAPI_Freezer<std::string>& freezer) {
    for(Component* child: *component->GetChildren()) {
        if ( PapiMetricsAttrib::existsMetricsAttrib(child) ) {
            logprintf("Component has metricsattrib");
            auto metricsAttrib = PapiMetricsAttrib::getMetricsAttrib(child);
            for(auto& eventSet: metricsAttrib->eventSets) {
                freezer.defrost();                
                //TODO use eventSet.eventIndices!
                logprintf("Freezing %d", eventSet.eventSet);
                storageManager.data(eventSet.eventSet, freezer);
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
    DefaultFreezer freezer;
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

