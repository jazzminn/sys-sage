#include "papi/Region.hpp"
#include "papi/Logging.hpp"

using namespace papi;

int Region::init(int cpuId) {
    int rv = STATUS_OK;
    int eventCount = 0;
    // validating configuration
    for(auto& event: configuration->events) {
        int eventId = PAPI_NULL;

        rv = PAPI_query_named_event(event.name.c_str());
        if ( rv != PAPI_OK ) {
            logprintf("Event %s not available, error: %d", event.name.c_str(), rv);
            return MEASUREMENT_ERROR_INVALID_EVENTNAME;
        }

        rv = PAPI_event_name_to_code(event.name.c_str(), &eventId);
        if ( rv != PAPI_OK ) {
            logprintf("Failed to determine event ID for name %s, error: %d", event.name.c_str(), rv);
            return MEASUREMENT_ERROR_INVALID_EVENTNAME;
        }

        if ( topology != nullptr ) {
            // determine sys-sage component
            auto info = Measurement::getComponentForEvent(event.name);
            // override option
            if ( !event.option.empty() ) {
                logprintf("Overriding component info %d/%d with option '%s'", info.componentType, info.level, event.option.c_str());
                if ( !info.override(event.option) ) {
                    logprintf("Failed to override component info, unsupported option '%s'", event.option.c_str());
                }
            }
            // find components by type
            std::vector<Component*> components;
            topology->FindAllSubcomponentsByType(&components, info.componentType);
            int componentCount = 0;
            for(auto component: components) {
                // special case, for cache components the level must be checked
                if ( info.componentType == SYS_SAGE_COMPONENT_CACHE ) {
                    Cache* cache = (Cache*)component;
                    if ( cache->GetCacheLevel() != info.level ) continue;
                }
                if ( cpuId != -1 ) {
                    // implicit binding to current CPU/thread
                    if ( !component_has_hwthread(component, cpuId) ) continue;
                }
                rv = eventSetManager.registerEvent(component, event.name, eventId, cpuId);
                if ( rv != STATUS_OK ) {
                    logprintf("Failed to register event %s for component: %d", event.name.c_str(), component->GetName(), rv);
                    return rv;
                }
                componentCount++;
            }
            if ( componentCount > 0 ) {
                logprintf("Added event %s to %d components", event.name.c_str(), componentCount);
            } else {
                logprintf("No component found for event %s", event.name.c_str());
                return MEASUREMENT_ERROR_INVALID_TOPOLOGY;
            }
        } else {
            //TODO implement PAPI metrics without topology
            logprintf("PAPI Metrics without Topology not yet supported.");
            return MEASUREMENT_ERROR_NOT_IMPLEMENTED;
        }
        eventCount++;
    }

    if ( eventCount == 0 ) {
        logprintf("Invalid configuration: no events");
        return MEASUREMENT_ERROR_INVALID_CONFIG;
    }

    return STATUS_OK;
}

bool Region::component_has_hwthread(Component* component, int hwThreadId) {
    std::vector<Component*> coreThreads;
    component->FindAllSubcomponentsByType(&coreThreads, SYS_SAGE_COMPONENT_THREAD);
    for(auto hwThread: coreThreads) {
        Thread* coreThread = (Thread*)hwThread;
        if ( hwThreadId == coreThread->GetId() ) return true;
    }
    return false;
}

int Region::deinit() {
    //TODO implement
    return STATUS_OK;
}
int Region::start() {
    return eventSetManager.startAll();
}
int Region::read() {
    //TODO implement
    return MEASUREMENT_ERROR_NOT_IMPLEMENTED;
}
int Region::stop() {
    return eventSetManager.stopAll();
}
int Region::save() {
    return eventSetManager.saveAll();
}
