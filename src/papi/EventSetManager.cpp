#include "papi/EventSetManager.hpp"
#include "papi/Logging.hpp"

// topology attrib names
static const char* attribMetrics = "papiMetrics";
//TODO implement additional config options
//static const char* envMetricsConfigMpxKey = "SYS_SAGE_METRICS_MPX";
//static const char* envMetricsConfigSysKey = "SYS_SAGE_METRICS_SYS";

using namespace papi;

int EventSetManager::registerEvent(Component* component, const std::string& eventName, int eventId, int cpuId, int tid) {
    int rv;
    int componentType = component->GetComponentType();
    int cpu = -1;
    if ( componentType > SYS_SAGE_COMPONENT_CHIP || cpuId != -1 ) {
        // no CPU binding
    } else {
        // binding to the first CPU
        std::vector<Component*> coreThreads;
        component->FindAllSubcomponentsByType(&coreThreads, SYS_SAGE_COMPONENT_THREAD);
        if ( coreThreads.size() == 0 ) {
            logprintf("Sys-sage component %s has no CPU child.", component->GetName().c_str());
            return MEASUREMENT_ERROR_INVALID_TOPOLOGY;
        }
        Thread* coreThread = (Thread*)coreThreads[0]; // use the first found thread component
        cpu = coreThread->GetId();
        //TODO validate cpu number by querying PAPI_get_hardware_info
    }

    rv = PAPI_get_event_component(eventId);
    if ( rv < PAPI_OK ) {
        logprintf("Event %s has no component, error: %d", eventName.c_str(), rv);
        return rv;
    }
    int papiComponent = rv;

    EventSetId id{(short)papiComponent, (short)cpu, tid};

    if ( eventSets.count(id.id) == 0 ) {
        // no eventset exists, creating
        int papiEventSet = PAPI_NULL;
        rv = PAPI_create_eventset(&papiEventSet);
        if ( rv != PAPI_OK ) {
            logprintf("Failed to create new eventset for papi component %d and cpu %d, error: %d", papiComponent, cpu, rv);
            return rv;
        }

        rv = PAPI_assign_eventset_component(papiEventSet, papiComponent);
        if ( rv != PAPI_OK ) {
            logprintf("Failed to assign new eventset to papi component %d, error: %d", papiComponent, rv);
            return rv;
        }

        if ( tid > 0 ) {
            rv = PAPI_attach(papiEventSet, tid);
            if ( rv != PAPI_OK ) {
                logprintf("Failed to attach tid %d to new eventset, error: %d", tid, rv);
                return rv;
            }
        } else if ( cpu > -1 ) {
            PAPI_option_t opts;
            opts.cpu.eventset = papiEventSet;
            opts.cpu.cpu_num = cpu;

            rv = PAPI_set_opt( PAPI_CPU_ATTACH, &opts );
            if ( rv != PAPI_OK ) {
                logprintf("Failed to attach CPU %d to new eventset, error: %d", cpu, rv);
                return rv;
            }
        }

        //FIXME set additional event set options here ... like multiplex
        //TODO rv = PAPI_set_multiplex( papiEventSet );

        eventSets[id.id] = EventSet{papiEventSet, papiComponent, cpu, tid};
        logprintf("Created new event set for component %d, cpu %d: tid: %d, id: %ld", papiComponent, cpu, tid, id.id);
    }


    rv = PAPI_add_event(eventSets[id.id].papiEventSet, eventId);
    if ( rv != PAPI_OK ) {
        logprintf("Failed to add event %s to event eventset, error: %d", eventName.c_str(), rv);
        return rv;
    }

    eventSets[id.id].counters.push_back(0);
    eventSets[id.id].events.emplace_back(eventId, eventName, component);

    logprintf("Added event %s to eventset with id: %ld", eventName.c_str(), id.id);
    return STATUS_OK;
}

int EventSetManager::startAll() {
    int rv;
    for(auto& entry: eventSets) {
        rv = PAPI_start(entry.second.papiEventSet);
        if ( rv != PAPI_OK ) {
            logprintf("Failed to start eventset %ld, error: %d", entry.first, rv);
            return rv;
        }
        entry.second.startTimeStamp = PAPI_get_real_usec();
    }
    logprintf("Started %ld event sets", eventSets.size());
    return STATUS_OK;
}

int EventSetManager::stopAll() {
    int rv;
    for(auto& entry: eventSets) {
        auto counters = entry.second.counters.data();
        rv = PAPI_stop(entry.second.papiEventSet, counters);
        if ( rv != PAPI_OK ) {
            logprintf("Failed to stop eventset %ld, error: %d", entry.first, rv);
            return rv;
        }
        entry.second.stopTimeStamp = PAPI_get_real_usec();
        for(size_t i=0; i<entry.second.events.size(); i++) {
            entry.second.events[i].readings.emplace_back(entry.second.stopTimeStamp-entry.second.startTimeStamp, entry.second.counters[i]);
        }
    }
    logprintf("Stopped %ld event sets", eventSets.size());
    return STATUS_OK;
}

int EventSetManager::saveAll() {
    for(auto& entry: eventSets) {
        for(auto& event: entry.second.events) {
            if ( event.component->attrib.count(attribMetrics) == 0 ) {
                event.component->attrib[attribMetrics] = (void*)new ComponentMetricsResults();
            }
            ComponentMetricsResults* results = (ComponentMetricsResults*)event.component->attrib[attribMetrics];
            if ( results->eventMetrics.count(event.name) == 0 ) {
                results->eventMetrics[event.name] = std::vector<MetricsResults>{};
            }
            for(auto& reading: event.readings) {
                results->eventMetrics[event.name].emplace_back(reading.timestamp, reading.value);
            }
        }
    }
    logprintf("Saved %ld event sets", eventSets.size());
    return STATUS_OK;
}

int EventSetManager::populateCountersForThread(int tid, std::vector<long long>& counters) {
    EventSetId id{0, -1, tid};
    if ( eventSets.count(id.id) == 0 ) {
        return MEASUREMENT_ERROR_TID_NOT_FOUND;
    }

    // populate counters with the last readings
    for(auto value: eventSets[id.id].counters) {
        counters.push_back(value);
    }
    return STATUS_OK;
}

int EventSetManager::attribXmlHandler(std::string key, void* value, xmlNodePtr n) {
    if(!key.compare(attribMetrics)) {
        xmlNodePtr attrib_node = xmlNewNode(NULL, (const unsigned char *)"Attribute");
        xmlNewProp(attrib_node, (const unsigned char *)"name", (const unsigned char *)"PapiMetrics");
        xmlAddChild(n, attrib_node);
       
        ComponentMetricsResults* results = (ComponentMetricsResults*)value;
        for(auto& entry: results->eventMetrics) {
            xmlNodePtr eventNode = xmlNewNode(NULL, (const unsigned char *)"Event");
            xmlNewProp(eventNode, (const unsigned char *)"name", (const unsigned char *)entry.first.c_str());
            xmlAddChild(attrib_node, eventNode);
            for(auto& reading: entry.second) {
                xmlNodePtr counterNode = xmlNewNode(NULL, (const unsigned char *)"Counter");
                auto elapsed = std::to_string(reading.elapsed);
                auto counter = std::to_string(reading.counter);
                xmlNewProp(counterNode, (const unsigned char *)"elapsed", (const unsigned char *)elapsed.c_str());
                xmlNewProp(counterNode, (const unsigned char *)"value", (const unsigned char *)counter.c_str());
                xmlAddChild(eventNode, counterNode);
            }
        }
        return 1;
    }
    return 0;
}

