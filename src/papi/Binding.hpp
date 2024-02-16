#pragma once

#include "papi/Metrics.hpp"

namespace papi {
    /// @brief Represents the subset of an eventSet
    class EventSetSubSet {
    public:
        int eventSet;
        std::vector<int> eventIndices;
        
        EventSetSubSet(int es, const std::vector<int>& ix);
        EventSetSubSet(int es);

        bool operator==(const EventSetSubSet& other) const;
    };

    /// @brief Represents the set of bound eventSets
    class PapiMetricsAttrib {
    public:
        static inline const char* attribMetrics = "papiMetrics";

        std::vector<EventSetSubSet> eventSets;

        static PapiMetricsAttrib* getMetricsAttrib(Component* component);
        static bool existsMetricsAttrib(Component* component);
        static void deleteMetricsAttrib(Component* component);
    };

    /// @brief Represents a set of data tables
    class PapiMetricsTable {
    public:
        static inline const char* attribMetricsTable = "papiMetricsTable";

        std::vector<SYSSAGE_PAPI_DataTable<std::string>> tables;

        static PapiMetricsTable* getMetricsTable(Component* component);
        static bool existsMetricsTable(Component* component);
        static void deleteMetricsTable(Component* component);
    };

}