#pragma once

#include "papi/Metrics.hpp"

namespace papi {

    typedef int(*PapiCounterFunctionPtr)(int, long long*);

    /// @brief Represents the extended type of a component
    class ComponentInfo {
    public:
        int componentType;
        // additional attributes
        int level = 0;

        static bool component_has_hwthread(Component* component, int hwThreadId);
        /// @brief Suggests a sys-sage Component type for a given event name
        /// @param eventName event name
        /// @return the Component info
        static ComponentInfo getComponentForEvent(const std::string& eventName);
    private:
        ComponentInfo(int t, int l = 0);
    };

    /// @brief Represents the configuration of an eventSet
    class EventSetInfo {
    public:
        int eventSet;
        std::vector<std::string> events;
        unsigned long tid;
        int core;

        EventSetInfo(int es, const std::vector<std::string>& e, unsigned long t = 0, int cpu = -1);
        EventSetInfo();
        EventSetInfo(const EventSetInfo& copy);

        bool operator==(const EventSetInfo& other) const;
        bool operator!=(const EventSetInfo& other) const;
        bool operator()(const EventSetInfo& a, const EventSetInfo& b) const;

        /// @brief Determines the configuration of the eventSet 
        /// @param eventSet PAPI event set
        /// @return a pair, the first is the status code, the second is the event set info, which is only valid if status was PAPI_OK
        static std::pair<int, EventSetInfo> createEventSetInfo(int eventSet);
    };

    /// @brief Represents a reading of an eventSet
    class EventSetReading {
    public:
        long long timestamp;
        int core;
        std::vector<long long> counters;

        EventSetReading(long long ts, int c, const std::vector<long long>& values);
    };

    /// @brief Represents a measurement session of an eventSet
    class EventSetStorageSession {
    public:
        long long startTimeStamp = 0;
        int startCore = -1;
        long long stopTimeStamp = 0;
        int stopCore = -1;
        std::vector<EventSetReading> readings;

        EventSetStorageSession(long long ts, int c);

        int stop(long long ts, int core);
        int add(long long ts, int core, const std::vector<long long>& counters);
    };

    /// @brief Handles the storage of an eventSet
    class EventSetStorage {
    public:
        std::vector<EventSetStorageSession> sessions;

        EventSetInfo eventSetInfo;

        EventSetStorage(const EventSetInfo& info);
        EventSetStorage();

        int addCounters(const std::vector<long long>& counters);

        int start();
        int stop();
    };

    /// @brief Manages eventSet measurements and data access
    class StorageManager {
        template<PapiCounterFunctionPtr papi_fn>
        int storePapiCounter(int eventSet);

    public:
        std::map<int, EventSetStorage> eventSetStorage;

        EventSetStorage* getStorage(int eventSet);

        int start(int eventSet);
        int store(int eventSet, const std::vector<long long>& values);
        int read(int eventSet);
        int accum(int eventSet);
        int stop(int eventSet);
        int destroy(int eventSet);

        int data(int eventSet, SYSSAGE_PAPI_Visitor& visitor);
        std::map<int, int> cpuStat(int eventSet);
    };

}