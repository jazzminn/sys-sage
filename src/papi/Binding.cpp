#include "papi/Binding.hpp"

using namespace papi;

EventSetSubSet::EventSetSubSet(int es, const std::vector<int>& ix) : eventSet{es}, eventIndices{ix} {}
EventSetSubSet::EventSetSubSet(int es) : eventSet{es} {}

bool EventSetSubSet::operator==(const EventSetSubSet& other) const {
    return eventSet == other.eventSet
        && eventIndices == other.eventIndices;
}

PapiMetricsAttrib* PapiMetricsAttrib::getMetricsAttrib(Component* component) {
    if ( !existsMetricsAttrib(component) ) {
        component->attrib[PapiMetricsAttrib::attribMetrics] = (void*)new PapiMetricsAttrib();
        //logprintf("Create MetricsAttrib for %s", component->GetName().c_str());
    }
    return (PapiMetricsAttrib*)component->attrib[PapiMetricsAttrib::attribMetrics];
}
bool PapiMetricsAttrib::existsMetricsAttrib(Component* component) {
    return component->attrib.count(PapiMetricsAttrib::attribMetrics) > 0;
}
void PapiMetricsAttrib::deleteMetricsAttrib(Component* component) {
    if ( existsMetricsAttrib(component) ) {
        PapiMetricsAttrib* pma = getMetricsAttrib(component);
        if ( pma != nullptr ) delete pma;
        component->attrib.erase(PapiMetricsAttrib::attribMetrics);
    }
}

PapiMetricsTable* PapiMetricsTable::getMetricsTable(Component* component) {
    if ( !existsMetricsTable(component) ) {
        component->attrib[PapiMetricsTable::attribMetricsTable] = (void*)new PapiMetricsTable();
        //logprintf("Create MetricsTable for %s", component->GetName().c_str());
    }
    return (PapiMetricsTable*)component->attrib[PapiMetricsTable::attribMetricsTable];
}
bool PapiMetricsTable::existsMetricsTable(Component* component) {
    return component->attrib.count(PapiMetricsTable::attribMetricsTable) > 0;
}
void PapiMetricsTable::deleteMetricsTable(Component* component) {
    if ( existsMetricsTable(component) ) {
        PapiMetricsTable* pmt = getMetricsTable(component);
        if ( pmt != nullptr ) delete pmt;
        component->attrib.erase(PapiMetricsTable::attribMetricsTable);
    }
}
