#include "papi/Storage.hpp"
#include "papi/Logging.hpp"
#include "papi/SystemInfo.hpp"

#include "xml_dump.hpp"

#include <papi.h>

#include <map>

using namespace papi;

ComponentInfo::ComponentInfo(int t, int l) : componentType{t}, level{l} {}

bool ComponentInfo::component_has_hwthread(Component* component, int hwThreadId) {
    Thread * t = (Thread*)component->FindSubcomponentById(hwThreadId, SYS_SAGE_COMPONENT_THREAD);
    return t != nullptr;
}

ComponentInfo ComponentInfo::getComponentForEvent(const std::string& eventName) {
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

EventSetInfo::EventSetInfo(int es, const std::vector<std::string>& e, unsigned long t, int cpu)
: eventSet{es}, events{e}, tid{t}, core{cpu}
{}

EventSetInfo::EventSetInfo()
: EventSetInfo{0, {}}
{}

EventSetInfo::EventSetInfo(const EventSetInfo& copy)
: EventSetInfo(copy.eventSet, copy.events, copy.tid, copy.core)
{}

bool EventSetInfo::operator==(const EventSetInfo& other) const {
    return eventSet == other.eventSet
        && events == other.events
        && tid == other.tid
        && core == other.core;
}

bool EventSetInfo::operator!=(const EventSetInfo& other) const {
    return eventSet != other.eventSet
        || events != other.events
        || tid != other.tid
        || core != other.core;
}

bool EventSetInfo::operator()(const EventSetInfo& a, const EventSetInfo& b) const {
    return a == b;
}

std::pair<int, EventSetInfo> EventSetInfo::createEventSetInfo(int eventSet) {
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

    EventSetReading::EventSetReading(long long ts, int c, const std::vector<long long>& values)
    : timestamp{ts}, core{c}, counters{values} 
    {}


EventSetStorageSession::EventSetStorageSession(long long ts, int c) : startTimeStamp{ts}, startCore{c} {}

int EventSetStorageSession::stop(long long ts, int core) {
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
int EventSetStorageSession::add(long long ts, int core, const std::vector<long long>& counters) {
    if ( startTimeStamp == 0 ) {
        return PAPI_ENOTRUN;
    }
    if ( stopTimeStamp != 0 ) {
        return PAPI_ENOTRUN;
    }
    readings.emplace_back(ts, core, counters);
    return PAPI_OK;
}


EventSetStorage::EventSetStorage(const EventSetInfo& info) : eventSetInfo{info} {}
EventSetStorage::EventSetStorage() {}

int EventSetStorage::addCounters(const std::vector<long long>& counters) {
    auto num_sessions = sessions.size();
    if ( num_sessions == 0 ) {
        logprintf("No storage session found!");
        return PAPI_ENOINIT;
    }
    int core = eventSetInfo.core != -1 ? eventSetInfo.core : papi::SystemInfo::getThreadCpu(eventSetInfo.tid);
    return sessions[num_sessions-1].add(TIMESTAMP_FUNCTION(), core, counters);
}

int EventSetStorage::start() {
    int core = eventSetInfo.core != -1 ? eventSetInfo.core : papi::SystemInfo::getThreadCpu(eventSetInfo.tid);
    sessions.emplace_back(TIMESTAMP_FUNCTION(), core);
    return PAPI_OK;
}

int EventSetStorage::stop() {
    auto num_sessions = sessions.size();
    if ( num_sessions == 0 ) {
        logprintf("No storage session found!");
        return PAPI_ENOINIT;
    }
    int core = eventSetInfo.core != -1 ? eventSetInfo.core : papi::SystemInfo::getThreadCpu(eventSetInfo.tid);
    sessions[num_sessions-1].stop(TIMESTAMP_FUNCTION(), core);
    return PAPI_OK;
}


EventSetStorage* StorageManager::getStorage(int eventSet) {
    if ( eventSetStorage.count(eventSet) > 0 ) {
        return &eventSetStorage[eventSet];
    }
    return nullptr;
}

int StorageManager::start(int eventSet) {
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


template<PapiCounterFunctionPtr papi_fn>
int StorageManager::storePapiCounter(int eventSet) {
    int rv;

    if ( eventSetStorage.count(eventSet) == 0 ) {
        logprintf("Cannot save counters, storage for EventSet %d not yet initialized", eventSet);
        return PAPI_ENOINIT;
    }

    auto& info = eventSetStorage[eventSet].eventSetInfo;
#ifdef STORE_CHECK_NEEDED
    auto check = EventSetInfo::createEventSetInfo(eventSet);
    if ( info != check.second ) {
        logprintf("EventSet has been changed, ignoring new counters");
        return SYSSAGE_PAPI_ECHANGED;
    }
#endif
    int num_counters = info.events.size();
    std::vector<long long> counters(num_counters, 0);
    rv = papi_fn(eventSet, counters.data());
    if ( rv != PAPI_OK ) {
        logprintf("Failed to stop|read|accum eventset, err: %d", rv);
        return rv;
    }

    return eventSetStorage[eventSet].addCounters(counters);
}

int StorageManager::store(int eventSet, const std::vector<long long>& values) {
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

int StorageManager::read(int eventSet) {
    if ( eventSetStorage.count(eventSet) == 0 ) {
        logprintf("Cannot stop storage, storage for EventSet %d not yet initialized", eventSet);
        return PAPI_ENOINIT;
    }

    return storePapiCounter<::PAPI_read>(eventSet);
}

int StorageManager::accum(int eventSet) {
    if ( eventSetStorage.count(eventSet) == 0 ) {
        logprintf("Cannot stop storage, storage for EventSet %d not yet initialized", eventSet);
        return PAPI_ENOINIT;
    }

    return storePapiCounter<::PAPI_accum>(eventSet);
}

int StorageManager::stop(int eventSet) {
    if ( eventSetStorage.count(eventSet) == 0 ) {
        logprintf("Cannot stop storage, storage for EventSet %d not yet initialized", eventSet);
        return PAPI_ENOINIT;
    }
    int rv = storePapiCounter<::PAPI_stop>(eventSet);
    if ( rv != PAPI_OK ) return rv;
    return eventSetStorage[eventSet].stop();
}

int StorageManager::destroy(int eventSet) {
    if ( eventSetStorage.count(eventSet) == 0 ) {
        logprintf("Cannot destroy storage, storage for EventSet %d not yet initialized", eventSet);
        return PAPI_ENOINIT;
    }

    // deleting storage
    eventSetStorage.erase(eventSet);

    int e = eventSet;
    return ::PAPI_destroy_eventset(&e);
}

int StorageManager::data(int eventSet, SYSSAGE_PAPI_Visitor& visitor) {
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

std::map<int, int> StorageManager::cpuStat(int eventSet) {
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
